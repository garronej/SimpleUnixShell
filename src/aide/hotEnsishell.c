/*
 * Copyright (C) 2002, Simon Nieuviarts
 *               2010, Gregory Mounie
 */

#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include "string.h"
#include "unistd.h"
#include <sys/types.h>
#include <sys/wait.h>
#include "processus.h"
#include <fcntl.h>
#include <wordexp.h>
#include "errno.h"


/* Definitions des variables globales et des structures */
#define VARIANTE "Jokers et environnements / pipes multiples"

/* Fonction qui parse la commande en argument, et remplace les
 * jokers et les variables d'environnement par leur valeur. */
static void parsed_cmd(char* const cmd[]){

	wordexp_t p;
	
	for(int j=0; cmd[j]!=0; j++){
		if(j==0)
			wordexp(cmd[j], &p, 0);
		else
			wordexp(cmd[j], &p, WRDE_APPEND);
	}
	
	if(execvp(p.we_wordv[0], p.we_wordv)==-1){
		perror("ensishell.c:35 --> Erreur lors de l'execution de la commande");
		exit(1);
	}
	
	wordfree(&p);
}

/* Fonction qui execute toute une serie d'instructions entrée en commande.
 * Elle permet de gérer les pipes multiples, ainsi que les redirections 
 * d'entree-sortie. '*/
static int exec_cmd(struct cmdline* l){

	/* On calcule la longueur de la sequence pour détecter la 
	   présence de pipe.*/
	int long_seq=0;
	for (int j=0; l->seq[j]!=0; j++)
		long_seq++;

	/* On déclare un tableau de pids correspondant à la longueur de la séquence. */
	pid_t pids[long_seq];
	/* Variables necssaires pour les eventuelles redirections d'entree sortie.*/
	int status, fichier_in, fichier_out;
	int nouv_tuyau[2], prec_tuyau[2];

	nouv_tuyau[0] = -1;
	nouv_tuyau[1] = -1;

	for(int i=0; i <= long_seq-1; i++){

		prec_tuyau[0] = nouv_tuyau[0];
		prec_tuyau[1] = nouv_tuyau[1];

		if(i <= long_seq-2)
			pipe(nouv_tuyau);
		
		if((pids[i]=fork())== 0){
			
			if (i == 0 && l->in){
				fichier_in = open(l->in, O_RDWR);	
				dup2(fichier_in, STDIN_FILENO);	
			}
			else{
				close(prec_tuyau[1]);
				dup2(prec_tuyau[0], STDIN_FILENO);
			}

			if ((i==long_seq-1) && l->out){
				fichier_out = open(l->out, O_CREAT | O_RDWR);	
				dup2(fichier_out, STDOUT_FILENO);	
			}
			else{
				close(nouv_tuyau[0]);
				dup2(nouv_tuyau[1], STDOUT_FILENO);
			}
			
			parsed_cmd(l->seq[i]);

			if(i == 0 && l->in) 
				close(fichier_in);

			if((i==long_seq-1) && l->out) 
				close(fichier_out);
		} 
		else if(pids[i]==-1){
			perror("ensishell.c:66 --> Echec lors de l'appel à fork()");
			exit(1);
		}
		close(prec_tuyau[0]);
		close(prec_tuyau[1]);
	}
			
	for(int i=0; i < long_seq; i++)
		if (!l->bg){
			if(waitpid(pids[i],&status,0)==-1){
				perror("ensishell.c:109 --> Echec lors de l'appel à waitpid()");
				exit(1);
			}
		}
		else 
			ajout_ps(pids[i], *(l->seq[0]));
	
	return 0;
}

int main(){

	printf("%s\n", VARIANTE);
	
	while (1) {
		struct cmdline *l;
		char *prompt = "ensishell>";

		l = readcmd(prompt);

		/* If input stream closed, normal termination */
		if (!l) {
			printf("exit\n");
			libere_liste_ps();
			exit(0);
		}

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) 
			printf("in: %s\n", l->in);
		if (l->out) 
			printf("out: %s\n", l->out);
		if (l->bg) 
			printf("background (&)\n");

		/* On étudie les commandes propres à ensishell : 
		   exit ou liste_ps. */
		if(l->seq[0]!=0){
			if (!strcmp(*(l->seq[0]), "exit")){
				libere_liste_ps();
				break;
			}
			else if (!strcmp(*(l->seq[0]), "liste_ps"))				
				aff_liste_ps();
			/* Si il ne s'agit pas d'exit ou liste_ps, on tente d'executer 
			   la commande. */
			else
				exec_cmd(l);
		}
	}
}
