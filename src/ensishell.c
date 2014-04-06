/*****************************************************
 * Copyright Grégory Mounié 2008-2013                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdlib.h>

#include "variante.h"
#include "readcmd.h"

#include <stdlib.h> /*  Pour les constantes EXIT_SUCCESS et EXIT_FAILURE */
#include <stdio.h> /*  Pour fprintf() */
#include <unistd.h> /*  Pour fork(), exec */
#include <errno.h> /*  Pour perror() et errno */
#include <sys/types.h> /*  Pour le type pid_t */
#include <sys/wait.h> /*  Pour wait() */
#include <string.h> /* pour strcmp*/
#include <glob.h> /* Pour glob */
#include <fcntl.h> /* Pour open */


#ifndef VARIANTE
#error "Variante non défini !!"
#endif

#define TRUE 1

//  La fonction create_process duplique le processus appelant et retourne
//    le PID du processus fils ainsi créé 
pid_t create_process(void);

// fonction créant un processus pour la commande courante et l'exécutant
int create_and_exec_process(struct cmdline *l, int index);


// tuyau courant
int tuyau[2];

// descripteur de fichier correspondant à la sortie du tuyau precedent
int sortie_prec = STDIN_FILENO; 


//ATS list doublement chainée pout gerer les proc en bg
struct T_cellule
{
    pid_t pid;
    char *cmd; /* Commande entrée*/
    struct T_cellule *suiv; 
    struct T_cellule *prec;
};

int ldc_taille( struct T_cellule *liste);
struct T_cellule *ldc_cree();
void ldc_libere(struct T_cellule **pl);
void ldc_affiche( struct T_cellule **pl );
void ldc_insere_fin( struct T_cellule **pliste, pid_t pid, char **cmd);
void ldc_supprime( struct T_cellule **pl, struct T_cellule *ele);


void ldc_afficheDebug( struct T_cellule *liste );


//Liste des processus.
struct T_cellule *liste;


int main(){
	printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

	struct cmdline *l;
	liste = ldc_cree();
	char *prompt = "ensiPrompt>";
	while(TRUE){
		/* Si la dernière commange etais un bg attendre quelque instant */
		usleep( 20000 );
		l = readcmd(prompt);

		/* If input stream closed, normal termination */
		if (!l) { printf("exit\n"); ldc_libere(&liste); exit(EXIT_SUCCESS); }

		/* Syntax error, read another command */
		if (l->err) { printf("error: %s\n", l->err); continue; }


		/* For each command of the pipe */
		for (int i = 0; l->seq[i] != NULL; i++) {
			if( i == 2 ) { printf("More than one pipe not supported"); continue;}
			create_and_exec_process(l, i);           
		}
	}
return EXIT_SUCCESS;
}



/*  La fonction create_process duplique le processus appelant et retourne
 *     le PID du processus fils ainsi créé */
pid_t create_process(void){
	/*  On crée une nouvelle valeur de type pid_t */
	pid_t pid;

	/*  On fork() tant que l'erreur est EAGAIN */
	do pid = fork(); while ((pid == -1) && (errno == EAGAIN));

	/*  On retourne le PID du processus ainsi créé */
	return pid;
}


int create_and_exec_process(struct cmdline *l, int index) {

	int fd;

	if(strcmp(l->seq[index][0], "jobs") == 0){ ldc_affiche(&liste); return EXIT_SUCCESS;}
	if(strcmp(l->seq[index][0], "exit") == 0) { ldc_libere(&liste); exit(EXIT_SUCCESS);}

	glob_t gl; 
	glob(l->seq[index][0], 2064, 0, &gl); 
	for(int k= 1; l->seq[index][k] !=0; k++) glob(l->seq[index][k], 7216, 0, &gl);
	char** commande = gl.gl_pathv;

	pipe(tuyau);

	pid_t pid = create_process();
	switch(pid){
		case -1:
			ldc_libere(&liste);
			exit(EXIT_FAILURE);
			break;
		case 0: //fils
			if (index == 0 && l->in) {

				fd = open(l->in, O_RDONLY, 0);
				if(fd == -1){ perror("open()"); return EXIT_FAILURE; }
				dup2(fd, STDIN_FILENO);		

			}else dup2(sortie_prec, STDIN_FILENO);

			if (l->seq[index+1] != 0) { close(tuyau[0]); dup2(tuyau[1], STDOUT_FILENO);}
			else if( l->out){

				fd = open(l->out, O_CREAT|O_WRONLY| O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP |  S_IWGRP |S_IROTH | S_IWOTH);
				if(fd == -1){ perror("open()"); return EXIT_FAILURE; }
				dup2(fd, STDOUT_FILENO);
			}

			if ( execvp(commande[0], commande) == -1) {
				printf("%s : commande introuvable\n", l->seq[index][0]);
				ldc_libere(&liste);
				exit(EXIT_FAILURE);
			}
			break;
		default: //père

			close(tuyau[1]);
			sortie_prec = tuyau[0];

			if (!l->bg && l->seq[index+1] == NULL){
				if (waitpid(pid, NULL, 0) == -1) {
					perror("wait :");
					ldc_libere(&liste);
					exit(EXIT_FAILURE);
				}
			}else if(l->bg) ldc_insere_fin(&liste, pid, l->seq[index]);

			break;
	}
	globfree(&gl);
	return EXIT_SUCCESS;
}


//
//Gestion de la liste des processus.
//


struct T_cellule *ldc_cree(void) {
	return NULL;
}

int ldc_taille( struct T_cellule *liste) {
	if (liste == NULL) return(0);

	struct T_cellule *liste_cour = liste;
	int i = 0;
	do {
		liste_cour = (*liste_cour).suiv;
		i++;
	} while (liste_cour != liste);
	return(i);
}

void ldc_libere(struct T_cellule **pl) {
	if (ldc_taille(*pl) == 0) return; 
	struct T_cellule *liste_cour=*pl;
	struct T_cellule *liste_suiv=(*liste_cour).suiv;
	do {
		free(liste_cour->cmd);
		free(liste_cour);
		liste_cour=liste_suiv;
		liste_suiv=(*liste_cour).suiv;
	} while (liste_cour != *pl);
	*pl = ldc_cree();
}

void ldc_affiche( struct T_cellule **pl ){
	int status, size;
	pid_t out;
	char state[9];
	struct T_cellule *liste_cour = *pl;

	size = ldc_taille(*pl);
	for( int i=1; i<= size; i++){
		if( i == 1 ) printf("   No     PID    STATE   CMD\n");

		out=waitpid(liste_cour->pid, &status, WNOHANG);

		if( out == -1){ perror("waitpid"); ldc_libere(pl); exit(EXIT_FAILURE); }
		else if( out == 0 ){
			strcpy(state, "Runing");
			// Le proc est en cour et non interrompu : on l'affiche

			printf("%5d%8d%9s   %s &\n",i, liste_cour->pid,state, liste_cour->cmd);

			liste_cour = liste_cour->suiv;
		}else{
			if (WIFEXITED(status)) strcpy(state, "End");
			else if (WIFSIGNALED(status)) strcpy(state, "Error");
			else if (WIFSTOPPED(status)) strcpy(state, "Stoped");

			printf("%5d%8d%9s   %s\n",i, liste_cour->pid,state, liste_cour->cmd);

			//On suprime le pocessus.
			liste_cour = liste_cour->suiv;
			ldc_supprime( pl, liste_cour->prec);
		}
	}
}


void ldc_insere_fin( struct T_cellule **pliste, pid_t pid, char **cmd) {
	int length = 0;
	for( int i=0; cmd[i] != NULL; i++){
		length += strlen(cmd[i]) + 1;
	}
	if (ldc_taille(*pliste) == 0 ) {
		*pliste = malloc(sizeof(struct T_cellule));

		(*pliste)->cmd = malloc( length*sizeof(char)+1);

		strcpy((*pliste)->cmd, cmd[0]);
		for( int i=1; cmd[i] != NULL; i++){
			strcat((*pliste)->cmd," "); 
			strcat((*pliste)->cmd,cmd[i]); 
		}

		(*pliste)->pid = pid;
		(*pliste)->prec = *pliste;
		(*pliste)->suiv = *pliste;

	} else {
		struct T_cellule *pcel = malloc(sizeof(struct T_cellule));

		pcel->cmd = malloc( length*sizeof(char)+1);

		strcpy(pcel->cmd, cmd[0]);
		for( int i=1; cmd[i] != NULL; i++){
			strcat(pcel->cmd," "); 
			strcat(pcel->cmd,cmd[i]); 
		}

		pcel->pid = pid;

		pcel->suiv = *pliste;
		pcel->prec = (*pliste)->prec;

		(*pliste)->prec->suiv = pcel;
		(*pliste)->prec = pcel;

	}
}

void ldc_supprime( struct T_cellule **pl, struct T_cellule *ele) {
	if (ldc_taille(*(pl)) == 1) {
		*(pl)=NULL;
	} else {
		if ( ele == *pl ) *pl = ele->suiv;
		ele->prec->suiv = ele->suiv;
		ele->suiv->prec = ele->prec;
	}
	free(ele->cmd);
	free(ele);
}


void ldc_afficheDebug( struct T_cellule *liste )
{
	printf("----------------\n");
	if (ldc_taille(liste) == 0) printf("La liste est vide"); 
	else{
		struct T_cellule *liste_cour = liste;
		do {
			printf("%3d  %s\n",(*liste_cour).pid,liste_cour->cmd );
			liste_cour = (*liste_cour).suiv;
		} while (liste_cour != liste);
	}
	printf("----------------\n");
	printf("\n");
}
