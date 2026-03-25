#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>

#define LOGIN_FILE "login"
#define BLACKLIST_FILE "blacklist"
#define CREATE_ERROR "Eroare la crearea fisierului\n"
#define CLOSE_ERROR "Eroare la close\n"
#define FOPEN_ERROR "Eroare la FOpen\n"
#define READ_FILE_ERROR "Eroare la readfile\n"
#define OPEN_ERROR "Eroare la open\n"

/* portul folosit */
#define PORT 4000

/* codul de eroare returnat de anumite apeluri */
extern int errno;

// Mutex lock
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct thData
{
  int idThread; // id-ul thread-ului tinut in evidenta de acest program
  int cl;       // descriptorul intors de accept
} thData;

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void raspunde(void *);

int main()
{
  struct sockaddr_in server; // structura folosita de server
  struct sockaddr_in from;
  int nr;       // mesajul primit de trimis la client
  int sd1, sd2; // descriptorul de socket
  int pid;
  int len;
  pthread_t th[100]; // Identificatorii thread-urilor care se vor crea
  int i = 0;

  /* crearea unui socket */
  if ((sd1 = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("[server]Eroare la socket().\n");
    return errno;
  }
  /* utilizarea optiunii SO_REUSEADDR */
  int on = 1;
  setsockopt(sd1, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  /* pregatirea structurilor de date */
  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));

  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
  server.sin_family = AF_INET;
  /* acceptam orice adresa */
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  /* utilizam un port utilizator */
  server.sin_port = htons(PORT);

  /* atasam socketul */
  if (bind(sd1, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[server]Eroare la bind().\n");
    return errno;
  }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen(sd1, 2) == -1)
  {
    perror("[server]Eroare la listen().\n");
    return errno;
  }

  /* servim in mod concurent clientii...folosind thread-uri */
  while (1)
  {
    int client;
    thData *td; // parametru functia executata de thread
    int length = sizeof(from);

    printf("[server]Asteptam la portul %d...\n", PORT);
    fflush(stdout);

    /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
    if ((client = accept(sd1, (struct sockaddr *)&from, &length)) < 0)
    {
      perror("[server]Eroare la accept().\n");
      continue;
    }

    /* s-a realizat conexiunea, se astepta mesajul */

    td = (struct thData *)malloc(sizeof(struct thData));
    td->idThread = i++;
    td->cl = client;

    pthread_create(&th[i], NULL, &treat, td);

  } // while
};
static void *treat(void *arg)
{
  struct thData tdL;
  tdL = *((struct thData *)arg);
  free(arg);
  printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
  fflush(stdout);
  pthread_detach(pthread_self());
  raspunde((struct thData *)&tdL);
  /* am terminat cu acest client, inchidem conexiunea */
  close(tdL.cl);
  return (NULL);
};

void raspunde(void *arg)
{
  int i = 0;
  char com[100], nume_fisier[21];
  struct stat st;
  int fd;
  int size;
  int client;
  int isLogged = 0;
  struct thData tdL;
  tdL = *((struct thData *)arg);
  while (1)
  {
    if (read(tdL.cl, com, sizeof(com)) <= 0)
    {
      printf("[Thread %d] Clientul s-a deconectat neasteptat.\n", tdL.idThread);
      break;
    }

    printf("[Thread %d]Mesajul a fost receptionat...%s\n", tdL.idThread, com);

    if (strstr(com, "login") != NULL)
    {
      char username[21];
      sscanf(com + 6, "%20s", username);

      int is_blacklisted = 0;
      int user_exists = 0;
      char storedPassword[65] = {0};

      pthread_mutex_lock(&file_lock);

      // Check blacklist
      if (access(BLACKLIST_FILE, F_OK) == -1)
      {
        int fd = open(BLACKLIST_FILE, O_RDWR | O_CREAT, 0666);
        if (fd == -1)
        {
          perror(CREATE_ERROR);
        }
        if (close(fd) == -1)
        {
          perror(CLOSE_ERROR);
        }
      }

      FILE *cfile = fopen(BLACKLIST_FILE, "r");
      if (cfile != NULL)
      {
        char bline[256];
        while (fgets(bline, sizeof(bline), cfile))
        {
          bline[(int)strlen(bline) - 1] = '\0';
          char *b = strstr(bline, username);
          if (b != NULL && b == bline)
          {
            is_blacklisted = 1;
            break;
          }
        }
        fclose(cfile);
      }

      // Check login file
      if (!is_blacklisted)
      {
        if (access(LOGIN_FILE, F_OK) == -1)
        {
          int fd = open(LOGIN_FILE, O_RDWR | O_CREAT, 0666);
          if (fd == -1)
          {
            perror(CREATE_ERROR);
          }
          if (close(fd) == -1)
          {
            perror(CLOSE_ERROR);
          }
        }

        FILE *file = fopen(LOGIN_FILE, "r");
        if (file != NULL)
        {
          char line[256];
          while (fgets(line, sizeof(line), file))
          {
            line[(int)strlen(line) - 1] = '\0';
            char *p = strstr(line, username);

            // If user exists store the password
            if (p != NULL && p == line)
            {
              user_exists = 1;
              strcpy(storedPassword, line + strlen(username) + 1);
              break;
            }
          }
          fclose(file);
        }
      }

      pthread_mutex_unlock(&file_lock);

      if (is_blacklisted)
      {
        strcpy(com, "Userul ");
        strcat(com, username);
        strcat(com, " nu este autorizat.\n");
        write(tdL.cl, com, sizeof(com));
        break;
      }
      else if (user_exists)
      {
        // User exists, ask for password
        strcpy(com, "Bine ai revenit ");
        strcat(com, username);
        strcat(com, ". Introduceti parola (max 20 caractere)\n");
        write(tdL.cl, com, sizeof(com));

        read(tdL.cl, com, sizeof(com));
        printf("[Thread %d]Mesajul a fost receptionat...%s\n", tdL.idThread, com);

        char password[65];
        strncpy(password, com, 64);
        password[64] = '\0';

        // Check password
        if (strcmp(password, storedPassword) == 0)
        {
          isLogged = 1;
          strcpy(com, "Login reusit.");
          write(tdL.cl, com, sizeof(com));
        }
        else
        {
          strcpy(com, "Parola incorecta.");
          write(tdL.cl, com, sizeof(com));
          printf("[Thread %d] Parola gresita. Inchidem conexiunea.\n", tdL.idThread);
          break;
        }
      }
      else
      {
        // User does not exist, create new account
        strcpy(com, "Bine ai venit ");
        strcat(com, username);
        strcat(com, ". Introdu noua parola (max 20 de caractere).\n");
        write(tdL.cl, com, sizeof(com));

        read(tdL.cl, com, sizeof(com));
        printf("[Thread %d]Mesajul a fost receptionat...%s\n", tdL.idThread, com);

        char password[65];
        strncpy(password, com, 64);
        password[64] = '\0';

        char line[256];
        sprintf(line, "%s %s\n", username, password);

        pthread_mutex_lock(&file_lock);

        FILE *file = fopen("login", "a");
        if (file != NULL)
        {
          fprintf(file, "%s", line);
          fclose(file);
        }

        pthread_mutex_unlock(&file_lock);

        isLogged = 1;
        strcpy(com, "Login reusit.");
        write(tdL.cl, com, sizeof(com));
      }
    }

    else if (strstr(com, "get") != NULL)
    {
      if (isLogged == 0)
      {
        strcpy(com, "Eroare: Nu sunteti autentificat.\n");
        write(tdL.cl, com, sizeof(com));
        continue;
      }
      sscanf(com + 4, "%20s", nume_fisier);

      if (strchr(nume_fisier, '/') != NULL || strchr(nume_fisier, '\\') != NULL || strstr(nume_fisier, "..") != NULL)
      {
        printf("[Security] Path traversal detectat: %s\n", nume_fisier);
        size = 0;
        send(tdL.cl, &size, sizeof(int), 0);
      }
      else
      {
        stat(nume_fisier, &st);
        fd = open(nume_fisier, O_RDONLY);

        if (fd == -1)
        {
          size = 0;
          send(tdL.cl, &size, sizeof(int), 0);
        }
        else
        {
          size = st.st_size;
          send(tdL.cl, &size, sizeof(int), 0);

          int bytes_sent = 0;
          char buffer[4096];

          while (bytes_sent < size)
          {
            int bytes_to_read = size - bytes_sent;
            if (bytes_to_read > sizeof(buffer))
            {
              bytes_to_read = sizeof(buffer);
            }

            int bytes_read = read(fd, buffer, bytes_to_read);
            if (bytes_read <= 0)
              break;

            int r = send(tdL.cl, buffer, bytes_read, 0);
            if (r <= 0)
              break;

            bytes_sent += r;
          }

          close(fd);
        }
      }
    }

    else if (strstr(com, "put") != NULL)
    {
      if (isLogged == 0)
      {
        strcpy(com, "Eroare: Nu sunteti autentificat.\n");
        write(tdL.cl, com, sizeof(com));
        continue;
      }

      int c = 0;
      sscanf(com + 4, "%20s", nume_fisier);
      recv(tdL.cl, &size, sizeof(int), 0);

      if (strchr(nume_fisier, '/') != NULL || strchr(nume_fisier, '\\') != NULL || strstr(nume_fisier, "..") != NULL)
      {
        printf("[Security] Path traversal detectat la upload: %s\n", nume_fisier);

        int bytes_received = 0;
        char buffer[4096];
        while (bytes_received < size)
        {
          int chunk = size - bytes_received;
          if (chunk > sizeof(buffer))
            chunk = sizeof(buffer);
          int r = recv(tdL.cl, buffer, chunk, 0);
          if (r <= 0)
            break;
          bytes_received += r;
        }

        c = 0;
        send(tdL.cl, &c, sizeof(int), 0);
      }

      else
      {
        i = 1;
        int count = 1;
        char base_name[21];
        strcpy(base_name, nume_fisier);

        while (1)
        {
          fd = open(nume_fisier, O_CREAT | O_EXCL | O_WRONLY, 0666);
          if (fd == -1)
          {
            char *dot = strrchr(base_name, '.');

            if (dot != NULL)
            {
              int name_len = dot - base_name;
              snprintf(nume_fisier, sizeof(nume_fisier), "%.*s(%d)%s", name_len, base_name, count, dot);
            }
            else
            {
              snprintf(nume_fisier, sizeof(nume_fisier), "%.8s(%d)", base_name, count);
            }
            count++;
          }
          else
            break;
        }

        int bytes_received = 0;
        char buffer[4096];

        while (bytes_received < size)
        {
          int chunk = size - bytes_received;
          if (chunk > sizeof(buffer))
            chunk = sizeof(buffer);

          int rec = recv(tdL.cl, buffer, chunk, 0);
          if (rec <= 0)
            break;

          c += write(fd, buffer, rec);
          bytes_received += rec;
        }

        close(fd);
        send(tdL.cl, &c, sizeof(int), 0);
      }
    }
    else if (strstr(com, "badd") != NULL)
    {
      if (isLogged == 0)
      {
        strcpy(com, "Eroare: Nu sunteti autentificat.\n");
        write(tdL.cl, com, sizeof(com));
        continue;
      }

      char usernameb[21];
      sscanf(com + 5, "%20s", usernameb);
      pthread_mutex_lock(&file_lock);
      if (access(BLACKLIST_FILE, F_OK) == -1)
      {
        int fd2 = open(BLACKLIST_FILE, O_RDWR | O_CREAT, 0666);

        if (fd2 == -1)
        {
          perror(CREATE_ERROR);
        }

        if (close(fd2) == -1)
        {
          perror(CLOSE_ERROR);
        }
      }

      FILE *bfile = fopen(BLACKLIST_FILE, "r");

      if (bfile == NULL)
      {
        perror(FOPEN_ERROR);
      }

      char line[256];
      int found = 0;
      char *poit;
      while ((poit = fgets(line, sizeof(line), bfile)))
      {
        if (poit == NULL)
        {
          perror(READ_FILE_ERROR);
        }
        line[(int)strlen(line) - 1] = '\0';

        char *p = strstr(line, usernameb);

        if (p != NULL && p == line)
        {
          strcpy(com, "Utilizatorul ");
          strcat(com, usernameb);
          strcat(com, " se afla deja in blacklist.\n");
          write(tdL.cl, com, sizeof(com));
          found = 1;
          break;
        }
      }

      if (fclose(bfile) == EOF)
      {
        perror(CLOSE_ERROR);
      }

      if (found == 0)
      {
        sprintf(line, "%s\n", usernameb);
        bfile = fopen("blacklist", "a");
        fprintf(bfile, "%s", line);
        fclose(bfile);

        strcpy(com, "User-ul a fost adaugat in blacklist.");
        write(tdL.cl, com, sizeof(com));
      }

      pthread_mutex_unlock(&file_lock);
    }
    else if (strstr(com, "quit") != NULL)
    {
      strcpy(com, "Conexiunea a fost inchisa.");
      write(tdL.cl, com, sizeof(com));
      break;
    }
  }
}
