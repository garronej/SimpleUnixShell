/*****************************************************
 * Copyright Grégory Mounié 2008-2013                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
 #include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <glob.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif


struct processus {
	char *name;
	pid_t pid;
	struct processus *next;
};
struct processus *proc;

void add_proc(struct cmdline *l, pid_t pid);
void remove_proc(pid_t pid);
void display_jobs();
void verif_jobs();

char** check_joker(char **cmd);
char* single_joker(char *cmd);


int main() {
    printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

	proc = NULL;

	while (1) {
		struct cmdline *l;
		int i;
		char *prompt = "\033[01;30mensishell \033[00m> ";

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
		

		// Il y a une commande
		if (l->seq[0] != 0) {
			char **cmd = l->seq[0];

			// On regarde si l'utilisateur veut quitter ou non
			if (strcmp(cmd[0], "exit") == 0 || strcmp(cmd[0], "quit") == 0) {
				exit(0);
			}
			// On regarde si la commande "jobs" est appelée
			else if (strcmp(cmd[0], "jobs") == 0) {
				display_jobs();
			}
			// Sinon on exécute la commande
			else {
				// Calcul le nombre de pipe
				int nb_pipe = 0;
				for (i=1; l->seq[i]!=0; i++) {
					nb_pipe++;
				}

				// Initialise les pipefd
				int pipefd[nb_pipe*2];
				for (i=0; i < nb_pipe; ++i)
				{
					pipe(pipefd + 2*i);
				}

				pid_t attente = 0;
				for (i=0; l->seq[i]!=0; i++) {
					char **cmd = l->seq[i];

					pid_t fils = fork();
					// Si on est dans le fils, alors on exécute la bonne cmd
					if(fils == 0) {
						// On attend que le pipe précédent soit terminé
						if (i > 0) {
							waitpid(attente, NULL, 0);
						}

						// On redirige la sortie
						if (i < nb_pipe) {
							dup2(pipefd[i*2 +1], 1);
						}
						else if (l->out) {
							int file = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRGRP | S_IROTH);
							dup2(file, 1);
							close(file);
						}

						// On redirige l'entrée
						if (i > 0) {
							dup2(pipefd[(i-1)*2], 0);
						}
						else if (l->in) {
							int file = open(l->in, O_RDONLY);
							dup2(file, 0);
							close(file);
						}

						// Ferme tous les pipefd
						for (int j=0; j < nb_pipe*2; ++j)
						{
							close(pipefd[j]);
						}

						// On regarde s'il y a des jokers
						// et on récupère les nouveaux paramètres
						char** param = check_joker(cmd);

						// Cmd
						execvp(cmd[0], param);
					}
					else {
						attente = fils;
					}
				}

				// Ferme tous les pipefd
				for (int j=0; j < nb_pipe*2; ++j)
				{
					close(pipefd[j]);
				}

				if(! l->bg) {
					waitpid(attente, NULL, 0);
				} 
				// Sinon, on l'ajoute à la liste des processus en bg
				else {
					add_proc(l, attente); // A MODIFIER
				}
			}
		}


		// Vérification des processus en background
		verif_jobs();
	}
}


/* Ajoute un processus dans la liste des processus en background */
void add_proc(struct cmdline *l, pid_t pid)
{
	char ***name = l->seq;

	struct processus *temp = proc;
	if (proc == NULL) {
		temp = malloc(sizeof(struct processus));
		proc = temp;
	}
	else {
		while (temp->next != NULL) {
			temp = temp->next;
		}

		temp->next = malloc(sizeof(struct processus));
		temp = temp->next;
	}

	// Ajout des champs
	int size = 0;
    // 1ere partie de la commande
	size += (int) strlen(name[0][0]);
	for (int j=1; name[0][j] != 0; j++) {
			size++;
	        size += (int) strlen(name[0][j]);
	}
	// On rajoute les pipes
	for (int i=1; name[i] != 0; i++) {
		size += 2;
		for (int j=0; name[i][j] != 0; j++) {
			size++;
	        size += (int) strlen(name[i][j]);
	    }
    }
    // On rajoute les redirections
    if (l->in) {
    	size += 10;
    	size += (int) strlen(l->in);
    }
    if (l->out) {
    	size += 11;
    	size += (int) strlen(l->out);
    }
    size++;
	temp->name = malloc(size * sizeof(char));

	// 1ere partie de la commande
	strcpy(temp->name, name[0][0]);
	for (int j=1; name[0][j] != 0; j++) {
			strcat(temp->name, " ");
	        strcat(temp->name, name[0][j]);
	}
	// On rajoute les pipes
	for (int i=1; name[i] != 0; i++) {
		strcat(temp->name, " |");
		for (int j=0; name[i][j] != 0; j++) {
			strcat(temp->name, " ");
	        strcat(temp->name, name[i][j]);
	    }
    }
    // On rajoute les redirections
    if (l->in) {
    	strcat(temp->name, " --- in : ");
    	strcat(temp->name, l->in);
    }
    if (l->out) {
    	strcat(temp->name, " --- out : ");
		strcat(temp->name, l->out);
    }
	temp->pid = pid;
	temp->next = NULL;
}

/* Enlève un processus de la liste des processus en background */
void remove_proc(pid_t pid)
{
	struct processus *temp = proc;
	if (proc != NULL) {
		// Si c'est le premier élément de la liste
		if (proc->pid == pid) {
			proc = proc->next;
		}
		else {
			struct processus *pere = proc;
			temp = proc->next;
			while (temp->pid != pid) {
				pere = temp;
				temp = temp->next;
			}

			pere->next = temp->next;
		}
		free(temp->name);
		free(temp);
	}
}

/* Affiche les processus en background */
void display_jobs() 
{
	struct processus *temp = proc;
	while (temp != NULL) {
		int status;
		pid_t pid = waitpid(temp->pid, &status, WNOHANG);

		if (pid == 0) {
			printf("[%d] - Running - %s \n", (int)temp->pid, temp->name);
		} 
		else {
			printf("[%d] - Done    - %s \n", (int)temp->pid, temp->name);
			remove_proc(temp->pid);
		}
		temp = temp->next;
	}
}

/* Vérifie si les processus sont terminés ou non
   Si un processus est terminé, cela sera affiché */
void verif_jobs()
{
	struct processus *temp = proc;
	while (temp != NULL) {
		int status;
		pid_t pid = waitpid(temp->pid, &status, WNOHANG);

		if (pid != 0) {
			printf("[%d] - Done    --- %s \n", (int)temp->pid, temp->name);
			remove_proc(temp->pid);
		}
		temp = temp->next;
	}
}


/* Retourne les paramètres modifiés selon les jokers */
char** check_param_joker(char **cmd) 
{
	char** res;
	char *g;
	size_t length = 0;
	glob_t glob_results;
	char *temp;

	// Analyse les jokers
	glob(cmd[1], GLOB_NOCHECK, 0, &glob_results);

	// Espace nécessaire
	for (int i=0; i < glob_results.gl_pathc; ++i) {
		temp = glob_results.gl_pathv[i];
		length += strlen(temp) + 1;
	}

	// Crée la nouvelle commande
	g = malloc(length * sizeof(char));
	for (int i=0; i < glob_results.gl_pathc; ++i) {
		temp = glob_results.gl_pathv[i];
		strcat(g, temp);
	}

	globfree(&glob_results);

	return g;
}

/* Retourne le paramètre modifié selon les jokers */
// char* single_joker(char *cmd)
// {
// 	char* star  = strchr(cmd, '*');
// 	char* tilde = strchr(cmd, '');
// 	char* brace = strchr(cmd, '');

// 	if (star != NULL) {
		
// 	}

// }



