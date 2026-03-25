#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <openssl/evp.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

void hash_password(const char *password, char *output_buffer)
{
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;

  EVP_MD_CTX *mdctx = EVP_MD_CTX_new();

  EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
  EVP_DigestUpdate(mdctx, password, strlen(password));
  EVP_DigestFinal_ex(mdctx, hash, &lengthOfHash);
  EVP_MD_CTX_free(mdctx);

  for (unsigned int i = 0; i < lengthOfHash; i++)
  {
    sprintf(output_buffer + (i * 2), "%02x", hash[i]);
  }
  output_buffer[64] = '\0';
}

int main(int argc, char *argv[])
{
  int sd;                    // descriptorul de socket
  struct sockaddr_in server; // structura folosita pentru conectare
  char com[100], nume_fisier[21], nume_util[21], pass[65], temp_input[256];
  struct stat st;
  int fd;
  int op, size, status;
  int i = 1;

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
  {
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  /* stabilim portul */
  port = atoi(argv[2]);

  /* cream socketul */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("Eroare la socket().\n");
    return errno;
  }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons(port);

  /* ne conectam la server */
  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }

  /* citirea mesajului */
  while (1)
  {
    printf("Introduceti numele de utilizator (max 20 caractere): ");
    scanf("%255s", temp_input);

    if (strchr(temp_input, '/') != NULL || strchr(temp_input, '\\') != NULL || strstr(temp_input, "..") != NULL)
    {
      printf("[Eroare] Numele fisierului nu poate contine caractere ('/', '\\' sau '..')!\n");
    }
    else if (strlen(temp_input) > 20)
    {
      printf("[Eroare] Numele introdus are mai mult de 20 de caractere!\n\n");
    }
    else
    {
      strcpy(nume_util, temp_input);
      break;
    }
  }
  strcpy(com, "login ");
  strcat(com, nume_util);
  send(sd, com, 100, 0);
  recv(sd, com, 100, 0);
  printf("[client]Mesajul primit este: %s\n", com);

  if (strstr(com, "nu este autorizat") != NULL)
  {
    printf("Deconectare...\n");
    close(sd);
    exit(1);
  }
  while (1)
  {
    scanf("%255s", temp_input);
    if (strlen(temp_input) > 20)
    {
      printf("[Eroare] Parola nu poate avea mai mult de 20 de caractere! Reintroduceti: ");
    }
    else
    {
      hash_password(temp_input, pass);
      break;
    }
  }
  strcpy(com, pass);
  send(sd, com, 100, 0);
  recv(sd, com, 100, 0);
  printf("[client]Mesajul primit este: %s\n", com);
  if (strstr(com, "incorecta") != NULL)
  {
    printf("Deconectare...\n");
    close(sd);
    exit(1);
  }

  while (1)
  {
    printf("[client]Alegeti numarul operatiei dorite:\n1- get\n2- put\n3- add to blacklist\n4- quit\n");
    fflush(stdout);

    if (scanf("%d", &op) != 1)
    {
      int c;
      while ((c = getchar()) != '\n' && c != EOF)
      {
        printf("Va rugam introduceti un numar valid!\n\n");
        continue;
      }
    }

    switch (op)
    {
    case 1:
    {
      while (1)
      {
        printf("Introduceti numele fiserului: ");
        scanf("%255s", temp_input);
        if (strlen(temp_input) > 20)
        {
          printf("[Eroare] Numele fisierului este prea lung!\n");
        }
        else
        {
          strcpy(nume_fisier, temp_input);
          break;
        }
      }
      strcpy(com, "get ");
      strcat(com, nume_fisier);
      send(sd, com, 100, 0);
      recv(sd, &size, sizeof(int), 0);
      if (!size)
      {
        printf("Fisierul nu exista pe server\n\n");
        break;
      }

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
        {
          chunk = sizeof(buffer);
        }

        int rec = recv(sd, buffer, chunk, 0);
        if (rec <= 0)
          break;

        write(fd, buffer, rec);
        bytes_received += rec;
      }
      close(fd);
      break;
    }
    case 2:
    {
      while (1)
      {
        printf("Introduceti numele fisierului: ");
        scanf("%255s", temp_input);
        if (strlen(temp_input) > 20)
        {
          printf("[Eroare] Numele fisierului este prea lung!\n");
        }
        else
        {
          strcpy(nume_fisier, temp_input);
          break;
        }
      }
      fd = open(nume_fisier, O_RDONLY);
      if (fd == -1)
      {
        printf("Fisierul nu exista in directorul local\n\n");
        break;
      }
      strcpy(com, "put ");
      strcat(com, nume_fisier);
      send(sd, com, 100, 0);
      stat(nume_fisier, &st);
      size = st.st_size;
      send(sd, &size, sizeof(int), 0);

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

        int r = send(sd, buffer, bytes_read, 0);
        if (r <= 0)
          break;

        bytes_sent += r;
      }
      close(fd);

      recv(sd, &status, sizeof(int), 0);
      if (status)
        printf("Fisierul a fost transferat cu succes\n");
      else
        printf("Transferul fisierului a esuat\n");
      break;
    }
    case 3:
      while (1)
      {
        printf("Introduceti numele utilizatorului: ");
        scanf("%255s", temp_input);
        if (strlen(temp_input) > 20)
        {
          printf("[Eroare] Numele de utilizator este prea lung!\n");
        }
        else
        {
          strcpy(nume_util, temp_input);
          break;
        }
      }
      strcpy(com, "badd ");
      strcat(com, nume_util);
      send(sd, com, 100, 0);
      recv(sd, com, 100, 0);
      printf("[client]Mesajul primit este: %s\n", com);
      break;

    case 4:
      strcpy(com, "quit");
      send(sd, com, 100, 0);
      recv(sd, com, 100, 0);
      printf("[client]Mesajul primit este: %s\n", com);
      close(sd);
      exit(0);
    }
  }
  close(sd);
}
