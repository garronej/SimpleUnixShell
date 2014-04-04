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

#ifndef VARIANTE
#error "Variante non défini !!"
#endif


/*  La fonction create_process duplique le processus appelant et retourne
 *     le PID du processus fils ainsi créé */
pid_t create_process(void)
{
	/*  On crée une nouvelle valeur de type pid_t */
	pid_t pid;

	/*  On fork() tant que l'erreur est EAGAIN */
	do pid = fork(); while ((pid == -1) && (errno == EAGAIN));

	/*  On retourne le PID du processus ainsi créé */
	return pid;
}

int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

	while (1) {
		struct cmdline *l;
		int i, j;
		char *prompt = "jojoLaCochone>";

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
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
		}

		char **cmd;
		if( (cmd = l->seq[0]) != NULL ){
			//cmd[0] nom du program
			//cmd[1] premier argument...

			pid_t pid = create_process();


			switch (pid) {
				/*  Si on a une erreur irrémédiable (ENOMEM dans notre cas) */
				case -1:
					perror("fork");
					return EXIT_FAILURE;
					break;
					/*  Si on est dans le fils */
				case 0:


					/*  On crée un tableau contenant le nom du programme, l'argument et le 
					 *         dernier "NULL" obligatoire  */

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
					}

					break;
			}



		}


	}
}
