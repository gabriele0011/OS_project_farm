#include "list.h"

//inserimento di un elemento in testa alla lista
void insert_node(node** list, const char* string)
{
      //allocazione
      node* new = (node*)malloc(sizeof(node)); //inserire controllo malloc
      //setting
      if(!new){ LOG_ERR(errno, "malloc in insert node fallita");}
      
      size_t len_string = strlen(string);
      new->str = (char*)calloc(len_string+1, sizeof(char));
      if (!new->str){ LOG_ERR(errno, "calloc in insert node fallita\n"); }
      strncpy(new->str, string, len_string);
      new->str[len_string] = '\0';

      if (*list == NULL) new->next = NULL;
      else new->next = *list;
      *list = new;
      return;
}

//estrazione testa della lista
node* extract_node(node** list)
{
      if (*list == NULL){
            return NULL;
      }
      node* temp;
      temp = *list;
      *list = (*list)->next;
      return temp;
}

//deallocazione  lista
void dealloc_list(node** list)
{
      if (*list == NULL) return;
      node* curr = *list;
      while(curr != NULL){
            node* temp = curr;
            curr = curr->next;
            free(temp->str);
            free(temp);
      }
      *list = NULL;
      return;
}

//ricerca di un nodo
node* search_node(node* list, const char* string)
{
      while (list != NULL){
            if (strcmp(list->str, string) == 0){
                  return list;
            }
            list = list->next;
      }
      return NULL;
}

//rimozione di un nodo
void remove_node(node** list, const char* string)
{
      //caso lisa vuota
      if (*list == NULL){
            return;
      }
      //caso un elemento in lista
      if((*list)->next == NULL ){
            if(strcmp((*list)->str, string) == 0){
                  node* temp = *list;
                  *list = NULL;
                  free(temp->str);
                  free(temp);
            }
            return;
      }
      //caso piu di un elemento in lista
      node* curr = *list;
      node* prev = curr;
      while (curr != NULL){
            if(strcmp(curr->str, string) == 0 ){
                  if(curr == *list) *list = curr->next;
                  else prev->next = curr->next;
                  free(curr->str);
                  free(curr);
                  return;
            }
            prev = curr;
            curr = curr->next;
      }
      return;
}

void print_list(node* temp)
{
      while(temp != NULL){
            printf("%s ", temp->str);
            temp = temp->next;
      }
      printf("\n");
      return;

}
