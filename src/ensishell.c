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


#ifndef VARIANTE
#error "Variante non défini !!"
#endif

//  La fonction create_process duplique le processus appelant et retourne
//    le PID du processus fils ainsi créé 

pid_t create_process(void);

//ATS list doublement chainée pout gerer les proc en bg
struct T_cellule
{
    pid_t pid;
    char *cmd; /* Commande entrée*/
    struct T_cellule *suiv; 
    struct T_cellule *prec;
};

extern int ldc_taille( struct T_cellule *liste);
extern struct T_cellule *ldc_cree();
extern void ldc_libere(struct T_cellule **pl);
extern void ldc_affiche( struct T_cellule **pl );
extern void ldc_insere_fin( struct T_cellule **pliste, pid_t pid, char *cmd);
extern void ldc_supprime( struct T_cellule **pl, struct T_cellule *ele);


void ldc_afficheDebug( struct T_cellule *liste );


int main() {
    printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);
    struct cmdline *l;
    int i, j;
    char *prompt = "ensiPrompt>";
    char **cmd;
    pid_t pid;
    struct T_cellule *liste = ldc_cree();

    //ldc_insere_fin(&liste,pid, cmd[0]);
    //ldc_affiche(&liste);
    //ldc_libere(&list);
    while (1) {
        //On attend un peut le temps des premiers input
        if( l->bg ) sleep(1);
        l = readcmd(prompt);
        /* If input stream closed, normal termination */
        if (!l) {
            printf("exit\n");
            exit(0);
        }

        if (l->err) {
            /* Syntax error, read another command */
            printf("error: %s\n", l->err);
            continue;
        }

        if (l->in) printf("in: %s\n", l->in);
        if (l->out) printf("out: %s\n", l->out);
        if (l->bg) printf("background (&)\n");

        /* Display each command of the pipe */
        for (i=0; l->seq[i]!=0; i++) {
            cmd = l->seq[i];
            printf("seq[%d]: ", i);
            for (j=0; cmd[j]!=0; j++) {
                printf("'%s' ", cmd[j]);
            }
            printf("\n");
        }

        if( (cmd = l->seq[0]) != NULL ){
            // On regarde si la commande "jobs" est appelée
            if ( !strcmp(cmd[0], "jobs")) ldc_affiche(&liste);
            else if( !strcmp(cmd[0], "exit")) break;
            else{
                pid = create_process();
                switch (pid) {
                    /*  Si on a une erreur irrémédiable (ENOMEM dans notre cas) */
                    case -1:
                        perror("fork");
                        return EXIT_FAILURE;
                        break;
                        /*  Si on est dans le fils */
                    case 0:
                        /*  On lance le programme  */
                        if (execvp(cmd[0], cmd) == -1) {
                            perror("execv");
                            return EXIT_FAILURE;
                        }
                        break;
                        /*  Si on est dans le père */
                    default:

                        if( !l->bg ){ 
                            if (waitpid(pid, NULL, 0) == -1) {
                                perror("wait :");
                                exit(EXIT_FAILURE);
                            }
                        }else{
                            //On ajoute a la liste des jobs en bg.
                            ldc_insere_fin(&liste,pid, cmd[0]);
                        }

                        break;
                }
            }
        }
    }
    ldc_libere(&liste);
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

void ldc_affiche( struct T_cellule **pl ) {
	int i=0;
	int status, pass;
	pid_t out;
	char state[9];

	printf("   No     PID    STATE   CMD\n");
	if (ldc_taille(*pl) != 0){
		struct T_cellule *liste_cour = *pl;
		do {
			pass = 0;
			out=waitpid(liste_cour->pid, &status, WNOHANG);
			if( out == -1){
				perror("waitpid");
				exit(1);
			}else if( out == 0 ){
				strcpy(state, "Runing");
				// Le proc est en cour et non interrompu : on l'affiche
				printf("%5d%8d%9s   %s\n",++i, liste_cour->pid,state, liste_cour->cmd);
				liste_cour = liste_cour->suiv;
			}else{
				if (WIFEXITED(status)) strcpy(state, "End");
				else if (WIFSIGNALED(status)) strcpy(state, "Error");
				else if (WIFSTOPPED(status)) strcpy(state, "Stoped");
				printf("%5d%8d%9s   %s\n",++i, liste_cour->pid,state, liste_cour->cmd);

				//On suprime le pocessus.
				if(liste_cour == *pl) *pl = NULL;
				ldc_supprime( &liste_cour, liste_cour);
				if (ldc_taille(liste_cour)==0) break;
				else if(*pl == NULL){ 
					pass = 1;
					*pl = liste_cour;
				}
			}
		}while(liste_cour != *pl || pass);
	}
	printf("\n");
}

void ldc_insere_fin( struct T_cellule **pliste, pid_t pid, char *cmd) {
	if (ldc_taille(*pliste) == 0 ) {
		*pliste = malloc(sizeof(struct T_cellule));
		(*pliste)->cmd = malloc( (strlen(cmd)+1)*sizeof(char));
		strcpy((*pliste)->cmd, cmd);
		(*pliste)->pid = pid;
		(*pliste)->prec = *pliste;
		(*pliste)->suiv = *pliste;

	} else {
		struct T_cellule *pcel = malloc(sizeof(struct T_cellule));

		pcel->cmd = malloc( (strlen(cmd)+1)*sizeof(char));
		strcpy(pcel->cmd, cmd);
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
