#include "database.h"


#define DbInitSize 10

dictionary * database;
char backing_up=0;
char isEmpty=1;
extern pthread_mutex_t mutex;
FILE * log_fp;

int read_file_entry(FILE *fp, dictionary * aux);
int write_file_entry(FILE *fp, dictionary * aux);

int error_and_die_db(const char *msg) {
  perror(msg);
  return -1;
}


int dictionary_init(){
	int i;
	//verificar se existe backup file, se existir fazer backup

	//se houver backup alocar o tamanho necessario ao backup
	database = (dictionary *) malloc(sizeof(dictionary));
	if (database==NULL)
		return -1;
	
	database->next=database;
	database->prev=database;
	isEmpty=1;
	return 0;

}


int log_init (const char * file_name, const char * mode){

	log_fp=fopen(file_name, mode);
	if(log_fp == NULL) return -1;
    clearerr(log_fp);
    return 0;
}

int write_log(dictionary *aux, char op, char overwrite){

	//TODO: get mutex.....
	//write operation
	printf("write_log:\nop=%c overwrite=%d aux.key=%d\n", op, overwrite, aux->key);
	int nritems = fwrite(&op,sizeof(char),1,log_fp); if(nritems!=1) error_and_die_db("write_log op failed\n");
	nritems = fwrite(&overwrite,sizeof(char),1,log_fp); if(nritems!=1) error_and_die_db("write_log overwrite failed\n");
	
	if( write_file_entry(log_fp, aux) < 0) return error_and_die_db("write_log write_file_entry failed\n");
	//TODO: give mutex
	fflush(log_fp);
	return 0;
}

int read_log(){
	dictionary aux;
	char op, overwrite;
	//TODO: get mutex.....
	//write operation
	printf("inside read_log\n");
	backing_up=1;
	while(1){
		int nritems = fread(&op,sizeof(char),1,log_fp); 
		if(nritems!=1){
			if (ferror(log_fp)) return error_and_die_db("read_log op failed\n");
			if (feof(log_fp)) break;
		}
		nritems = fread(&overwrite,sizeof(char),1,log_fp);
		if(nritems!=1){
			if (ferror(log_fp)) return error_and_die_db("read_log op failed\n");
			if (feof(log_fp)) break;
		}
		if( read_file_entry(log_fp, &aux) < 0){
			if (ferror(log_fp)) return error_and_die_db("read_log op failed\n");
			else if (feof(log_fp)) break;
			else return -1;
		}
		printf("read_log:\nop=%c overwrite=%d aux.key=%d\n", op, overwrite, aux.key);
		switch (op){
			case 'w':
				if(add_entry(aux.key, aux.value, aux.value_length, overwrite) < 0) return error_and_die_db("read_log add_entry failed\n");
				break;
			case 'd':
				if(delete_entry(aux.key) < 0) return error_and_die_db("read_log delete_entry failed\n");
				break;
		}
	}
	backing_up=0;
	printf("EOF read_log\n");
	fclose(log_fp);
	//TODO: give mutex
	return 0;
}

dictionary * find_entry(uint32_t key){
	dictionary *aux = database;
	int i=0;

	pthread_t tid = pthread_self();

	if(isEmpty) return NULL;


	//Critical region
	if(aux->key==key) return aux;

	while(aux->next != database){
		pthread_mutex_lock(&mutex);
		aux = aux->next;
		pthread_mutex_unlock(&mutex);
		if(aux->key==key) {
			return aux;
		}
	}
	return NULL;
}

/** add_entry function*************************
@return -2 if entry already exists and not overwrite
@return 0 if no errors
@return -1 if errors found
************************************************/
int add_entry(uint32_t key, void * value, uint32_t value_length, int overwrite ){

	dictionary *new_entry;
	dictionary *entry_found;

	pthread_t tid = pthread_self();


	pthread_mutex_lock(&mutex);
	if(isEmpty) {
		database->key = key;
		database->value = value;
		database->value_length = value_length;
		isEmpty = 0;
	}
	pthread_mutex_unlock(&mutex);

	entry_found=find_entry(key);
	if(entry_found!=NULL){//if entry exists
		
		if (overwrite){
			pthread_mutex_lock(&mutex);
			free(entry_found->value);
			entry_found->value_length=value_length;//refresh value lenght
			entry_found->value = value;
			pthread_mutex_unlock(&mutex);
			//if (!backing_up)
				//if(write_log(entry_found, 'w', (char) overwrite) < 0) error_and_die_db("add_entry, overwrite entry, saving log\n");
			return 0;
		}else {//if already exists and not overwrite -> do nothing
			return -2;
		}

	}else{//add new entry
		isEmpty=0;

		new_entry = (dictionary *) malloc(sizeof(dictionary));
		new_entry->value = value;
		pthread_mutex_lock(&mutex);
		new_entry->prev=database->prev;
		new_entry->next=database;//new entry next point to first entry
		database->prev->next=new_entry;//new entry point to the last entry
		database->prev=new_entry;//1st entry prev point to last entry
		new_entry->key=key;
		new_entry->value_length=value_length;
		pthread_mutex_unlock(&mutex);

		//if (!backing_up)
			//if(write_log(new_entry, 'w',(char) overwrite) < 0) error_and_die_db("add_entry, new entry, saving log\n");
		return 0;
	}

		return -1;
}

/*
@return 1 if entry not exists
@return 0 if entry sucessfuly removed
@return -1 if error ocurred
*/
int delete_entry(uint32_t key){

	pthread_t tid = pthread_self();


	dictionary *aux;
	aux=find_entry(key);
	//printList();
	//printf("deleteing key %d\n", aux->key); fflush(stdout);
	if (aux!=NULL){
		pthread_mutex_lock(&mutex);
		aux->prev->next=aux->next;
		aux->next->prev=aux->prev;
		if(aux==database)//if the first node is deleted
			database = aux->next;

		//if (!backing_up)
			//if(write_log(aux, 'd', 0) < 0) error_and_die_db("delete_entry, saving log\n");

		free(aux->value);
		free(aux);
		pthread_mutex_unlock(&mutex);
		return 0;		
	}else{
		return -1;
	} 
}

/* 
Warning: Do not forget to free the memory after read_entry function
@param entry points to found entry in order to be read
@return 1 if entry not exists
@return 0 if entry sucessfuly read
@return -1 if error ocurred
*/
int read_entry(uint32_t key, dictionary ** entry){

	pthread_t tid = pthread_self();

	dictionary *aux;
	aux = find_entry(key);
	if(aux!=NULL){
		*entry = (dictionary*) malloc(sizeof(dictionary));
		//pthread_mutex_lock(&mutex);
		memcpy(*entry, aux, sizeof(dictionary));
		//pthread_mutex_unlock(&mutex);
		return 0;
	}else{
		return -2;
	}
	return -1;

}

void printList() {//only for debug purpose

	dictionary* aux = database;
	printf("key = %d\tvalue = %s\n", aux->key, (char*)aux->value);
	while(aux->next!=database) {
		aux = aux->next;
		printf("key = %d\tvalue = %s\n", aux->key, (char*)aux->value);	
	}
}

int write_file_entry(FILE *fp, dictionary * aux){
	int nritems;

	//write key and value length
	nritems = fwrite(&(aux->key),sizeof(uint32_t),1,fp); if(nritems!=1) return -1;
	nritems = fwrite(&(aux->value_length),sizeof(uint32_t),1,fp); if(nritems!=1) return -1;
	//write value
	nritems = fwrite(aux->value,aux->value_length,1,fp); if(nritems!=1) return -1;
	//printf("key = %d\tvalue = %s\n", aux->key, (char*)aux->value);	
	return 0;
}

int read_file_entry(FILE *fp, dictionary * aux){
	int nritems;
	void * temp_value;

	nritems = fread(&(aux->key),sizeof(uint32_t),1,fp);//write key and value length
	if(nritems!=1){
		//printf("fread key reached eof\n"); 
		return -1;
	}
	nritems = fread(&(aux->value_length),sizeof(uint32_t),1,fp);//read key and value length
	if(nritems!=1){
		//printf("fread value_length reached eof\n"); 
		return -1;
	}
	//printf("key = %d\tvalue length = %d\n", aux->key, aux->value_length);
	temp_value = malloc(aux->value_length);
	nritems = fread(temp_value,aux->value_length,1,fp);//read value
	if(nritems!=1){
		//printf("fread value reached eof\n"); 
		return -1;
	}
	//printf("value=%c\n", *(char *)temp_value);
	aux->value=temp_value;
	return 0;
}

int create_backup(const char * file_name){

    dictionary* aux = database;

    FILE *fp = fopen(file_name, "wb");
    if(fp == NULL) return -1;

    printf("backing up...\n");
    if(write_file_entry(fp, aux) < 0) return -1;

    while(aux->next!=database) {
		aux = aux->next;
		if(write_file_entry(fp, aux) < 0) return -1;
	}
	fclose(fp);
	return 0;

}

int read_backup(const char * file_name){

	dictionary aux;

    FILE *fp = fopen(file_name, "rb");
    if(fp == NULL) return -1;
    clearerr(fp);

    printf("reading backup...\n");
    backing_up=1;
    while(1) {
		if(read_file_entry(fp, &aux)<0) break;
		if(add_entry(aux.key, aux.value, aux.value_length, 1)<0) return -1;	
	}
	backing_up=0;
	fclose(fp);
	return 0;

}

int close_log(){
	return fclose(log_fp);
}

/*
void main(void){
	dictionary *entry;
	int * value;
	uint32_t key=10;
	dictionary * read_value;

	value = (int *) malloc(sizeof(int));
	*value =150;

	if(dictionary_init()==-1)
		printf("init error\n");

	printf("after init\n");

	if(add_entry(key, (void *) value,sizeof(int),0)==-1)
		printf("add entry error\n");

	printf("after add\n");

	entry = find_entry(10);
	if(entry==NULL)
		printf("entry null\n");
	else
		printf("key=%d value=%d value_length=%d\n", entry->key, *((int *)entry->value), entry->value_length );

	if(add_entry(50,(void *)&value,sizeof(int),0)==-1)
		printf("add entry error\n");

	entry = find_entry(50);
	if(entry==NULL)
		printf("entry null\n");
	else
		printf("key=%d value=%d value_length=%d\n", entry->key, *((int *)entry->value), entry->value_length );

	
	if(delete_entry(key))
		printf("Remove: value not exists\n");
	else
		printf("value removed\n");
	entry = find_entry(10);
	if (entry!=NULL)
		printf("key=%d value=%d value_length=%d\n", entry->key, *((int *)entry->value), entry->value_length );
	else
		printf("Not exists\n");

	if(read_entry(50, &read_value)==0)
		printf("read_value=%d\n", (*(int *)read_value));
	else
		printf("read_value:error\n");
}*/
