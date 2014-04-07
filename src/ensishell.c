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
#include <sys/time.h> /* Pour gettimeofday */


#ifndef VARIANTE
#error "Variante non défini !!"
#endif

#define TRUE 1


//  La fonction create_process duplique le processus appelant et retourne
//    le PID du processus fils ainsi créé 
pid_t create_process(void);

// fonction créant un processus pour la commande courante et l'exécutant
int create_and_exec_process(struct cmdline *l, int index);


// Tuyaux permétant au père et au fils de communiquer.
int tuyau[2];

// descripteur de fichier correspondant à la sortie de 
// la commande avant le pipe.
int sortie_prec = STDIN_FILENO; 


//list doublement chainée pout gerer les proc en bg
//Le tas a ete adapter pour les besoint de l'exercice.
struct T_cellule{
	pid_t pid; 
	char *cmd; /* Commande entrée*/
	int ended; /* vrais si le proc est términé. */
	struct timeval start; /* Moment du lancement du proc */
	int status; /* Si il c'est tesminé, avec quelle status. */
	struct T_cellule *suiv; 
	struct T_cellule *prec;

};

int ldc_taille( struct T_cellule *liste);
struct T_cellule *ldc_cree();
void ldc_libere(struct T_cellule **pl);
void ldc_supprime( struct T_cellule **pl, struct T_cellule *ele);

//Affiche la liste de processus avec leurs status et suprime ceux qui 
//sont terminer.
void ldc_affiche( struct T_cellule **pl );

void ldc_insere_fin( struct T_cellule **pliste, pid_t pid, char **cmd, struct timeval start);

//Recherche un processus avec le pid passer en entré et renvoi la cellule
//correspondante.
struct T_cellule *ldc_find( struct T_cellule *pl, pid_t pid);




void hdl (int sig, siginfo_t *siginfo, void *context);

void ldc_afficheDebug( struct T_cellule *liste );


//Liste des processus.
struct T_cellule *liste;

char *bgEndReport = NULL;

char *prompt = "\033[01;30mensishell \033[00m>";

int main(){
	printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

	struct cmdline *l;
	prompt = "\033[01;30mensishell \033[00m>" ;

	liste = ldc_cree();

	struct sigaction act;
	memset (&act, '\0', sizeof(act));
	/* Use the sa_sigaction field because the handles has two additional parameters */
	act.sa_sigaction = &hdl;

	/* The SA_SIGINFO flag tells sigaction() to use the sa_sigaction field, not sa_handler. */
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGCHLD, &act, NULL) < 0) {
		perror ("sigaction");
		exit(EXIT_FAILURE);
	}

	//system("reset");
	printf( "%s", prompt );

	while(TRUE){
		/* Si la dernière commange etais un bg attendre quelque instant */

			l = readcmd(" ");
			usleep(20000);
			if( l->bg == TRUE ) printf( "%s", prompt );
			free(bgEndReport);
			bgEndReport = NULL;

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


	if(strcmp(l->seq[index][0], "jobs") == 0){ ldc_affiche(&liste); printf( "%s", prompt ); return EXIT_SUCCESS;}
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

				//O_RDONLY; 
				fd = open(l->in, 0, 0);
				if(fd == -1){ perror("open()"); return EXIT_FAILURE; }
				dup2(fd, STDIN_FILENO);		

			}else dup2(sortie_prec, STDIN_FILENO);

			if (l->seq[index+1] != 0) { close(tuyau[0]); dup2(tuyau[1], STDOUT_FILENO);}
			else if( l->out){

				// O_CREAT|O_WRONLY|O_TRUNC
				//S_IRUSR | S_IWUSR | S_IRGRP |  S_IWGRP |S_IROTH | S_IWOTH
				fd = open(l->out, 577, 438);
				if(fd == -1){ perror("open()"); return EXIT_FAILURE; }
				dup2(fd, STDOUT_FILENO);
			}
			usleep(20000);

			//On attend que le proses ai bien ete ajouter a la liste  
			//des porcess en attente s'il devais l'étre avant de lancé l'execution.
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

				if (waitpid(pid, NULL, 0) == -1 && errno != EINTR) {
					perror("wait :");
					ldc_libere(&liste);
					exit(EXIT_FAILURE);
				}
				usleep(20000);
				write( STDOUT_FILENO,prompt, strlen(prompt)+1);

			}else if(l->bg){ 
				struct timeval start;
				gettimeofday(&start,NULL);
				ldc_insere_fin(&liste, pid, l->seq[index], start);
				break;
			}
			globfree(&gl);
	}
	return EXIT_SUCCESS;
}


void hdl (int sig, siginfo_t *siginfo, void *context) {
	int status;
	struct timeval end;
	gettimeofday(&end,NULL);
	char buffer[10];

	struct T_cellule *cel = ldc_find( liste, siginfo->si_pid );

	if( cel != NULL ){


		cel->ended = TRUE;


		bgEndReport = malloc( (100 + strlen(cel->cmd))*sizeof(char));
		strcpy( bgEndReport, "\nEnded : ");

		sprintf( buffer,"%d" ,cel->pid);
		strcat( bgEndReport, buffer );
		strcat( bgEndReport, "   " );
		strcat( bgEndReport, cel->cmd );
		strcat( bgEndReport, "   Exec time : " );

		sprintf( buffer,"%d" ,(int)(end.tv_sec - cel->start.tv_sec));
		strcat( bgEndReport,  buffer);
		strcat( bgEndReport,  "s ");

		sprintf( buffer,"%d" ,(int)((end.tv_usec - cel->start.tv_usec)/1000000));
		strcat( bgEndReport, buffer );
		strcat( bgEndReport, " ms\n" );
		strcat( bgEndReport, prompt );

		if (waitpid(cel->pid, &status, 0) == -1) { perror("waitpid"); ldc_libere(&liste); exit(EXIT_FAILURE); }
		else cel->status = status;

		write(STDOUT_FILENO, bgEndReport, strlen(bgEndReport));

	}
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


void ldc_affiche( struct T_cellule **pl ){
	int size;
	char state[9];
	struct T_cellule *liste_cour = *pl;

	size = ldc_taille(*pl);
	for( int i=1; i<= size; i++){
		if( i == 1 ) printf("   No     PID    STATE   CMD\n");

		if( !liste_cour->ended ){
			strcpy(state, "Runing");
			// Le proc est en cour et non interrompu : on l'affiche

			printf("%5d%8d%9s   %s &\n",i, liste_cour->pid,state, liste_cour->cmd);

			liste_cour = liste_cour->suiv;
		}else{
			if (WIFEXITED(liste_cour->status)) strcpy(state, "End");
			else if (WIFSIGNALED(liste_cour->status)) strcpy(state, "Error");
			else if (WIFSTOPPED(liste_cour->status)) strcpy(state, "Stoped");

			printf("%5d%8d%9s   %s\n",i, liste_cour->pid,state, liste_cour->cmd);

			//On suprime le pocessus.
			liste_cour = liste_cour->suiv;
			ldc_supprime( pl, liste_cour->prec);
		}
	}
}


void ldc_insere_fin( struct T_cellule **pliste, pid_t pid, char **cmd, struct timeval start) {
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

		(*pliste)->start = start;
		(*pliste)->ended = 0;

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

		pcel->start = start;
		pcel->ended = 0;

		pcel->pid = pid;

		pcel->suiv = *pliste;
		pcel->prec = (*pliste)->prec;

		(*pliste)->prec->suiv = pcel;
		(*pliste)->prec = pcel;

	}
}

struct T_cellule *ldc_find( struct T_cellule *pl, pid_t pid){

	if (ldc_taille(liste) == 0) return NULL; 
	else{
		struct T_cellule *liste_cour = liste;
		do {
			if( pid == liste_cour->pid ) return liste_cour;

			liste_cour = liste_cour->suiv;
		}while (liste_cour != liste);
	}
	return NULL;
}

