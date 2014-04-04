#include "processus.h"

#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include "unistd.h"
#include <sys/wait.h>

/* Les processus en background sont stockés dans une liste chainée */
static processus_t* tete = NULL;

int aff_liste_ps(){
	processus_t* cour = tete;
	processus_t* prec = NULL;
	int status = -1;
	printf(" - Liste des processus en background - \n");
	while (cour != NULL){
		if(waitpid(cour->pid, &status, WNOHANG)==-1){
			perror("processus.c:18 --> Echec lors de l'appel à waitpid()");
			exit(1);
		}
		/* Si le processus n'est pas terminé ou interrompu on l'affiche*/
		if (!WIFEXITED(status) || !WIFSIGNALED(status)){
			printf("   * PID : %d ", cour->pid);
			printf("- Nom : %s \n", cour->nom);
			
		}
		/* Sinon on le supprime de la liste.*/
		else {
			sup_ps(cour, prec);
		}	
		status = -1;
		prec = cour;
		cour = cour->suiv;
	}
	return 0;
}

int sup_ps(processus_t* cour, processus_t* prec){
	free(cour->nom);
	if (prec == NULL)
		tete=cour->suiv;
	else
		prec=cour->suiv;
	free(cour);
	return 0;
}

int libere_liste_ps(){
	processus_t* cour = tete;
	while(cour!=NULL){
		tete=tete->suiv;
		free(cour->nom);
		free(cour);
		cour=tete;
	}

	return 0;
}

int ajout_ps(pid_t pid, char* nom){
	processus_t* tmp = malloc(sizeof(processus_t));
	tmp->pid = pid;
	tmp->nom = malloc(strlen(nom)+1);
	strcpy(tmp->nom, nom);
	tmp->suiv = tete;
	tete = tmp;
	return 0;
}
