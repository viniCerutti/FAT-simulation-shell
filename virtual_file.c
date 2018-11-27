#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define CLUSTER_SIZE 1024
#define FAT_SIZE 4096
#define ROOT_NAME "root"
typedef struct{
	uint8_t filename[18];
	uint8_t attributes;
	uint8_t reserved[7];
	uint16_t first_block;
	uint32_t size;
} dir_entry_t;

union data_cluster{
	dir_entry_t dir[CLUSTER_SIZE / sizeof(dir_entry_t)];
	uint8_t data[CLUSTER_SIZE];
};

uint16_t fat[FAT_SIZE];

void init(){

	FILE *ptr_myfile;

	ptr_myfile=fopen("fat.part","wb");

	if (!ptr_myfile){
		printf("Impossivel abrir o arquivo!");
		return;
	}

	union data_cluster boot_block;

	int i;
	for (i = 0; i < CLUSTER_SIZE; i++){
		boot_block.data[i] = 0xbb;
	}

	uint16_t fat_init[FAT_SIZE];

	// alocar endereço na fat para boot block
	fat_init[0] = 0xfffd; 

	// alocar endereço fat para a propria fat
	for (i = 1; i <= 8; i++){
		fat_init[i] = 0xfffe; 
	}

	// alocar endereço na fat para root block
	fat_init[9] = 0xffff;

	// para o restante dos endereços na fat
	for (i = 10; i < FAT_SIZE; i++){
		fat_init[i] = 0x0000; 
	}

	union data_cluster clusters;

	//printf("teste");
	fwrite(&boot_block, CLUSTER_SIZE, 1, ptr_myfile);
	fwrite(&fat_init, sizeof(fat_init), 1, ptr_myfile);

	// salva no arquivos cluster root + outros clusters
	for (i = 0; i < 4086; i++){
		fwrite(&clusters, CLUSTER_SIZE, 1, ptr_myfile);
	}
	fclose(ptr_myfile);

}


void load (){
	FILE *ptr_myfile;

	ptr_myfile=fopen("fat.part","rb");

	if (!ptr_myfile){
		printf("Impossivel abrir o arquivo!");
		return;
	}

	union data_cluster boot_block;

	//carrega o boot_block para a memoria
	fread(&boot_block,CLUSTER_SIZE,1,ptr_myfile);
	int i;

	for (i = 0; i < CLUSTER_SIZE; i++){
		if (boot_block.data[i] != 0xbb){
			printf ("Problemas no endereços do boot_block");
			return;
		}

	}
	//carrega a fat para a memoria
	fread(&fat, sizeof(fat), 1, ptr_myfile);

	fclose(ptr_myfile);
}

union data_cluster __readCluster__(int index){
	FILE *ptr_myfile;

	ptr_myfile=fopen("fat.part","rb");
	union data_cluster cluster;

	if (!ptr_myfile){
		printf("Impossivel abrir o arquivo!");
		return cluster;
	}

	fseek(ptr_myfile,( CLUSTER_SIZE * index), SEEK_SET);
	fread(&cluster,sizeof(union data_cluster),1,ptr_myfile);

	fclose(ptr_myfile);

	return cluster;

}

void ls (char* directories){
	char * token;
	token = strtok(directories,"/"); // pega o primeiro elemento apos root

	union data_cluster block;

	if (directories[0] == '/'){
		block = __readCluster__(9);
	}else{
		printf("Caminho não possui diretório root!");
		return;
	}

	// procura o diretorio atual no anterior, se encontrar
	// então pode-se procurar o proximo diretorio neste de agora
	// acontece isso até chegar no último diretório que no final
	// teremos o os diretorios / arquivos deste diretório.

	while( token != NULL ) {
		//printf( " %s\n", token );
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i ++){

			if (strcmp(block.dir[i].filename,token) == 0){
				found_dir = 1;
				if(block.dir[i].attributes == 1){
					block = __readCluster__(block.dir[i].first_block);
				}else{
					printf("%s não é um diretório!\n",token);
				}
				break;
			}
		}
		if (!found_dir){
			printf("Não existe este diretório %s\n",token);
		}

		token = strtok(NULL, "/");
	}

	// block possui agora o cluster do último diretório

	int i;
	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	for (i = 0; i < size_dir; i ++){
		if(block.dir[i].first_block != 0){
			printf("%s\n",block.dir[i].filename);
		}
	}

}

void __writeCluster__(int index, union data_cluster *cluster){
    FILE *ptr_myfile;

    ptr_myfile=fopen("fat.part","rb+");

    if (!ptr_myfile){
        printf("Impossivel abrir o arquivo!");
        return;
    }

    fseek(ptr_myfile,(index*CLUSTER_SIZE), SEEK_SET);
    fwrite(cluster,CLUSTER_SIZE,1,ptr_myfile);

    fclose(ptr_myfile);
}

int __findFreeSpaceFat__(){
	int i;
	for (i = 0; i < FAT_SIZE; i++){
		 if (fat[i] == 0){
		 	return i;
		 }
	}

	return -1;
}

void __writeFat__(){
	FILE *ptr_myfile;

	ptr_myfile=fopen("fat.part","rb+");

	if (!ptr_myfile){
	    printf("Impossivel abrir o arquivo!");
	    return;
	}

	fseek(ptr_myfile,(CLUSTER_SIZE), SEEK_SET);
	fwrite(&fat,FAT_SIZE,1,ptr_myfile);

	fclose(ptr_myfile);
}


void unlink(char* directories){
	char dir_copy[strlen(directories)];
	strcpy(dir_copy,directories);

	char * token;
	token = strtok(directories,"/"); // pega o primeiro elemento apos root
	
	int index_block_fat = 0;

	union data_cluster block;

	if (directories[0] == '/'){
		index_block_fat = 9;
		block = __readCluster__(9);
	}else{
		printf("Caminho não possui diretório root!");
		return;
	}
	
	int count = 0;

	// conta quantos direitorios há na string
	while(token != NULL){
		//printf("%s\n",token);
		token = strtok(NULL,"/"); 
		count++;
	}

	//printf("\n");
	token = strtok(dir_copy,"/");
	//printf("casa");
	//printf("count = %d", count);

	// caminha nos diretórios até chegar no ultimo 
	// no qual é o que deve ser criado
	while( count > 1){;
		//printf("%s\n",token);
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i ++){

			if (strcmp(block.dir[i].filename,token) == 0){
				found_dir = 1;
				if(block.dir[i].attributes == 1){
					index_block_fat = block.dir[i].first_block;
					block = __readCluster__(block.dir[i].first_block);
				}else{
					printf("%s não é um diretório!\n",token);
				}
				break;
			}
		}

		if (!found_dir){
			printf("Não existe este diretório %s\n",token);
			return;
		}

		token = strtok(NULL,"/"); 
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int i;
	int found_unlink = 0;
	// tendo o diretorio no qual queremos criar o novo (token)
	// basta verificar se nao existe um aqruivo com este mesmo nome
	// verificar se possui um bloco livre no diretório e na fat
	for (i = 0; i < size_dir; i++){

		if (strcmp(block.dir[i].filename, token) == 0){

			if(block.dir[i].attributes == 1){
				found_unlink = 1;
				union data_cluster cluster_dir = __readCluster__(block.dir[i].first_block);

				int j;
				for (j = 0; j < size_dir; j++){
					if(cluster_dir.dir[j].first_block != 0){
						printf("\nDiretório ainda possui elementos!\n");
						return;
					}
				}

				memset(&cluster_dir,0x0000,CLUSTER_SIZE);
				fat[block.dir[i].first_block] = 0x0000;

				__writeCluster__(block.dir[i].first_block,&cluster_dir);
				memset(&block.dir[i],0x0000,sizeof(dir_entry_t));

				__writeCluster__(index_block_fat,&block);
				__writeFat__();
				break;

			}else if(block.dir[i].attributes == 0){
				printf("Hello world!");
				found_unlink = 1;


				uint16_t current = block.dir[i].first_block;
				printf("%d",current);
				uint16_t temp = block.dir[i].first_block;
				// vai apagando os clusters do segundo até o penultimo cluster
				while (fat[current] != 0xffff){
					union data_cluster cluster_dir = __readCluster__(current);
					memset(&cluster_dir,0x0000,CLUSTER_SIZE);
					__writeCluster__(current,&cluster_dir);
					temp = fat[current];
					fat[current] = 0x0000;
					current = temp;
				}

				union data_cluster cluster_dir = __readCluster__(current);
				memset(&cluster_dir,0x0000,CLUSTER_SIZE);
				__writeCluster__(current,&cluster_dir);
				fat[current] = 0x0000;

				memset(&block.dir[i],0x0000,sizeof(dir_entry_t));
				__writeCluster__(index_block_fat,&block);
				__writeFat__();
				break;
				
			}
		}
		
	}

	if(!found_unlink){
		printf("\nArquivo não encontrado!\n");
		return;
	}
}

// código adaptado de https://stackoverflow.com/questions/26620388/c-substrings-c-string-slicing
void __slice_str__(char * str, char * buffer, int start, int end)
{
    int j = 0;
    for ( int i = start; i < end; i++ ) {
        buffer[j++] = str[i];
    }
    buffer[j] = 0;
}

void write(char * words, char* directories){
	char dir_copy[strlen(directories)];
	strcpy(dir_copy,directories);

	char * token;
	token = strtok(directories,"/"); // pega o primeiro elemento apos root
	
	int index_block_fat = 0;

	union data_cluster block;

	if (directories[0] == '/'){
		index_block_fat = 9;
		block = __readCluster__(9);
	}else{
		printf("Caminho não possui diretório root!");
		return;
	}
	
	int count = 0;

	// conta quantos direitorios há na string
	while(token != NULL){
		//printf("%s\n",token);
		token = strtok(NULL,"/"); 
		count++;
	}

	//printf("\n");
	token = strtok(dir_copy,"/");
	//printf("casa");
	//printf("count = %d", count);

	// caminha nos diretórios até chegar no ultimo 
	// no qual é o que deve ser criado
	while( count > 1){;
		//printf("%s\n",token);
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i ++){

			if (strcmp(block.dir[i].filename,token) == 0){
				found_dir = 1;
				if(block.dir[i].attributes == 1){
					index_block_fat = block.dir[i].first_block;
					block = __readCluster__(block.dir[i].first_block);
				}else{
					printf("%s não é um diretório!\n",token);
				}
				break;
			}
		}

		if (!found_dir){
			printf("Não existe este diretório %s\n",token);
			return;
		}

		token = strtok(NULL,"/"); 
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int i;
	int found_unlink = 0;
	// tendo o diretorio no qual queremos criar o novo (token)
	// basta verificar se nao existe um aqruivo com este mesmo nome
	// verificar se possui um bloco livre no diretório e na fat
	for (i = 0; i < size_dir; i++){

		if (strcmp(block.dir[i].filename, token) == 0){

			if(block.dir[i].attributes == 0){
				printf("Hello world!");
				found_unlink = 1;


				uint16_t current = block.dir[i].first_block;
				printf("%d",current);
				uint16_t temp = block.dir[i].first_block;

				// vai apagando os clusters do segundo até o penultimo cluster
				while (fat[current] != 0xffff){
					union data_cluster cluster_dir = __readCluster__(current);
					memset(&cluster_dir,0x0000,CLUSTER_SIZE);
					__writeCluster__(current,&cluster_dir);
					temp = fat[current];
					fat[current] = 0x0000;
					current = temp;
				}

				union data_cluster cluster_dir = __readCluster__(current);
				memset(&cluster_dir,0x0000,CLUSTER_SIZE);
				block.dir[i].first_block = current;
				__writeCluster__(current,&cluster_dir);
				int number_clusters = ceil(sizeof(words)/(CLUSTER_SIZE * 1.0));


				//if (number_clusters == 1){
				//	memcpy(cluster_dir.data,words,sizeof(char) * strlen(words));
				//}

				int len = strlen(words);
    			char buffer[len + 1];
				// 
				// enquanto verdade
					// escrevo [0 + i * cluster_size : 1024 + i * cluster_size] no cluster_dir
					// escrevo cluster_dir no arquivo
					// fat[current] = 0xffff
					// i++
					// calculo a sobra i < number_clusters
						// se tiver sobra 
							// temp = procuro um index na fat de cluster livre
							// fat[current] = temp
							// current = temp
						// senão
						//	break;

				while(1){
					printf("teste");
					int offset = i * CLUSTER_SIZE;
					__slice_str__(words, buffer, offset, CLUSTER_SIZE + offset);

					cluster_dir = __readCluster__(current);
					printf("%d\n",current);
					printf("%s\n",buffer);
					memcpy(cluster_dir.data,buffer,sizeof(char) * strlen(buffer));
					__writeCluster__(current,&cluster_dir);

					fat[current] = 0xffff;
					i++;

					if (i < number_clusters){
						int next_index_fat = __findFreeSpaceFat__();

						if( next_index_fat == -1 ){
							printf("\nFat não possui espaço vazio!\n");
							return;
						}

						fat[current] = next_index_fat;
						current = next_index_fat;

					}else{
						break;
					}
				}

				__writeCluster__(index_block_fat,&block);
				__writeFat__();
				break;
			}
				
		}
		
	}

	if(!found_unlink){
		printf("\nArquivo não encontrado!\n");
		return;
	}
}

void mkdir(char* directories){
	char dir_copy[strlen(directories)];
	strcpy(dir_copy,directories);

	char * token;
	token = strtok(directories,"/"); // pega o primeiro elemento apos root
	
	int index_block_fat = 0;

	union data_cluster block;

	if (directories[0] == '/'){
		index_block_fat = 9;
		block = __readCluster__(9);
	}else{
		printf("Caminho não possui diretório root!");
		return;
	}
	
	int count = 0;

	// conta quantos direitorios há na string
	while(token != NULL){
		//printf("%s\n",token);
		token = strtok(NULL,"/"); 
		count++;
	}

	//printf("\n");
	token = strtok(dir_copy,"/");
	//printf("casa");
	//printf("count = %d", count);

	// caminha nos diretórios até chegar no ultimo 
	// no qual é o que deve ser criado
	while( count > 1){;
		//printf("%s\n",token);
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i ++){

			if (strcmp(block.dir[i].filename,token) == 0){
				found_dir = 1;
				if(block.dir[i].attributes == 1){
					index_block_fat = block.dir[i].first_block;
					block = __readCluster__(block.dir[i].first_block);
				}else{
					printf("%s não é um diretório!\n",token);
				}
				break;
			}
		}

		if (!found_dir){
			printf("Não existe este diretório %s\n",token);
			return;
		}

		token = strtok(NULL,"/"); 
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int i;

	// tendo o diretorio no qual queremos criar o novo (token)
	// basta verificar se nao existe um aqruivo com este mesmo nome
	// verificar se possui um bloco livre no diretório e na fat
	for (i = 0; i < size_dir; i++){

		if (strcmp(block.dir[i].filename, token) == 0){
			//printf("%s",token);
			printf("\nJá possui pasta neste diretório com este mesmo nome!\n");
				return;
		}

		if (block.dir[i].first_block == 0){
			
			int index_fat = __findFreeSpaceFat__();

			if(index_fat == -1 ){
				printf("\nFat não possui espaço vazio!\n");
				return;
			}

			fat[index_fat] = 0xffff;
			dir_entry_t new_dir;
			// limpa o novo diretorio a ser criado (apaga os lixos da memoria)

			memset(&new_dir,0x00,32);
			memcpy(new_dir.filename,token,sizeof(char) * strlen(token));
			new_dir.attributes = 1;
			new_dir.first_block = index_fat;
			new_dir.size = 0;

			// salva este novo diretorio no bloco do pai
			block.dir[i] = new_dir;
			//printf("%d",index_block_fat);
			//printf("\nindex fat = %d\n",index_fat);
			__writeCluster__(index_block_fat,&block);
			__writeFat__();
			break;
		}
	}

}

void create(char* directories){
    char dir_copy[strlen(directories)];
    strcpy(dir_copy,directories);
    char * token;
    token = strtok(directories,"/"); // pega o primeiro elemento apos root
    int index_block_fat = 0;
    union data_cluster block;
    if (directories[0] == '/'){
        index_block_fat = 9;
        block = __readCluster__(9);
    }else{
        printf("Caminho não possui diretório root!");
        return;
    }
    int count = 0;
    // conta quantos direitorios há na string
    while(token != NULL){
        //printf("%s\n",token);
        token = strtok(NULL,"/");
        count++;
    }
    //printf("\n");
    token = strtok(dir_copy,"/");
    //printf("count = %d", count);
    // caminha nos diretórios até chegar no penúltimo
    // no qual é o arquivo que deve ser criado
    while( count > 1){;
       // printf("%s\n",token);
        int i;
        int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
        int found_dir = 0;
        // procura o diretorio atual no anterior
        for (i = 0; i < size_dir; i ++){
            if (strcmp(block.dir[i].filename,token) == 0){
                found_dir = 1;
                if(block.dir[i].attributes == 1){
                    index_block_fat = block.dir[i].first_block;
                    block = __readCluster__(block.dir[i].first_block);
                }else{
                    printf("%s não é um diretório!\n",token);
                }
                break;
            }
        }
        if (!found_dir){
            printf("Não existe este diretório %s\n",token);
            return;
        }
        token = strtok(NULL,"/");
        count--;
    }
    int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
    int i;
    // tendo o diretorio no qual queremos criar o novo arquivo (token)
    // basta verificar se nao existe um arquivo com este mesmo nome
    // verificar se possui um bloco livre no diretório e na fat
    for (i = 0; i < size_dir; i++){
        if (strcmp(block.dir[i].filename, token) == 0){
            //printf("%s",token);
            printf("\nJá possui uma entrada neste diretório com este mesmo nome!\n");
            return;
        }
        if (block.dir[i].first_block == 0){
            int index_fat = __findFreeSpaceFat__();
            if(index_fat == -1 ){
                printf("\nFat não possui espaço vazio!\n");
                return;
            }
            fat[index_fat] = 0xffff;
            dir_entry_t new_arq;
            // limpa o novo arquivo a ser criado (apaga os lixos da memoria)
            memset(&new_arq,0x00,sizeof(dir_entry_t));
            memcpy(new_arq.filename,token,sizeof(char) * strlen(token));
            new_arq.attributes = 0;
            new_arq.first_block = index_fat;
            new_arq.size = 0;
            // salva este novo arquivo no bloco do pai
            block.dir[i] = new_arq;
            //printf("%d",index_block_fat);
            //printf("\nindex fat = %d\n",index_fat);
            __writeCluster__(index_block_fat,&block);
            __writeFat__();
            break;
        }
    }
}

void __loadFat__(){
	FILE *ptr_myfile;

	ptr_myfile=fopen("fat.part","rb");

	if (!ptr_myfile){
		printf("Impossivel abrir o arquivo!");
		return;
	}

	fseek(ptr_myfile,CLUSTER_SIZE,SEEK_SET);
	//carrega a fat para a memoria
	fread(&fat, sizeof(fat), 1, ptr_myfile);
	fclose(ptr_myfile);

	int i;

	for (i=0; i < FAT_SIZE; i++){
		printf("%d -> 0x%04x\n",i,fat[i]);
	}
}

int main()
{
   //init();
   load();
   char teste[7] = "/media";
   char teste2[17] = "/media/oi.txt";
   char teste3[6] = "amem";
   //mkdir(teste);
   //create(teste2);
   write(teste3,teste2);
   ls(teste);
   return 0;
}