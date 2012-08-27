#include <gnome-keyring.h>
#define OK GNOME_KEYRING_RESULT_OK

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

/* for password prompt */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static char *
default_user(void)
{
  int i;
  char *vars[] = { "USER", "LOGNAME", NULL };
  char *ret = NULL;
  for (i = 0; vars[i] && !ret; i++)
    ret = getenv(vars[i]);
  return ret;
}

static int
key_listing(void)
{
  GList *keyrings, *current;
  if (gnome_keyring_list_keyring_names_sync(&keyrings) != OK) {
    fprintf(stderr, "Couldn't get list of keyring names\n");
    return 1;
  }

  for (current = keyrings; current; current = current->next) {
    GList *ids, *cid;
    gchar *keyring = (gchar *)current->data;
    printf("Keyring %s\n", keyring);

    if (gnome_keyring_list_item_ids_sync(keyring, &ids) != OK) {
      fprintf(stderr, "Couldn't list IDs on keyring %s\n", keyring);
      continue;
    }

    for (cid = ids; cid; cid = cid->next) {
      GnomeKeyringItemInfo *info;
      if (gnome_keyring_item_get_info_sync(keyring, GPOINTER_TO_INT(cid->data), &info) == OK) {
        printf(" %s\n", gnome_keyring_item_info_get_display_name(info));
        gnome_keyring_item_info_free(info);
      }
    }
  }

  return 0;
}

#define MAXPASS 65536
static char *
password_prompt(const char *user, const char *server, const char *domain, const char *protocol, guint32 port)
{
  int status;
  int pid;
  int fd[2];
  char *password;
  password = (char *)malloc(MAXPASS * sizeof(char));
  if (!password) return password;
  password[0] = '\0';

  pipe(fd);

  pid = fork();
  if (pid < 0) return NULL;

  if (pid) { /* parent */
    int ret;
    close(fd[1]);
    ret = read(fd[0], password, MAXPASS);
    close(fd[0]);
    waitpid(-1, &status, 0);
    if (ret < 0 || !*password) password[0] = '\0';
    for (ret = 0; password[ret]; ret++) {
      if (password[ret] == '\r' || password[ret] == '\n') {
        password[ret] = '\0';
        break;
      }
    }
  } else {
    char *argv[] = { "dmenu", "--secret", "-p", "password", NULL };
    close(fd[0]);
    dup2(fd[1], 1);
    close(0);
    execvp("dmenu", argv);
    exit(1);
  }
  return password;
}

#define ARG(X) (!strcmp(argv[i], "-" #X))
#define LARG(X) (!strcmp(argv[i], "--" #X))
#define CHECK_ARGS if (i + 1 == argc) { fprintf(stderr, "Missing a required argument\n"); return 1; }
#define S_OPT(X) CHECK_ARGS X = argv[++i]
#define S_OPT_Q(X) S_OPT(X); set_ ## X = 1
#define I_OPT(X) CHECK_ARGS X = strtol(argv[++i], NULL, 0)
#define I_OPT_Q(X) I_OPT(X); set_ ## X = 1
int main(int argc, char **argv) {
  GnomeKeyringResult result;
  GList *results;
  char *host;
  int i, tried, verbose = 0, remove = 0, list = 0;
  int set_domain = 0, set_host = 0, set_server = 0;
  int set_protocol = 0, set_port = 0;

  char *user = default_user();
  char *domain = "thorleyindustries.com";
  char *server = "accpac";
  char *object = NULL;
  char *protocol = "rdp";
  char *authtype = NULL;
  guint32 port = 3389;

  char *use_keyring = GNOME_KEYRING_DEFAULT;

  for (i = 1; i < argc; i++) {
    if (ARG(u) || LARG(user)) {
      S_OPT(user);
    } else if (ARG(h) || LARG(host)) {
      S_OPT_Q(host);
    } else if (ARG(s) || LARG(server)) {
      S_OPT_Q(server);
    } else if (ARG(d) || LARG(domain)) {
      S_OPT_Q(domain);
    } else if (ARG(P) || LARG(protocol)) {
      S_OPT_Q(protocol);
    } else if (ARG(p) || LARG(port)) {
      I_OPT_Q(port);
    } else if (ARG(v) || LARG(verbose)) {
      verbose = 1;
    } else if (ARG(R) || LARG(remove)) {
      remove = 1;
    } else if (ARG(l) || LARG(list)) {
      list = 1;
    } else if (ARG(k) || LARG(keyring)) {
      S_OPT(use_keyring);
    } else {
      fprintf(stderr, "Unknown argument: %s\n", argv[i]);
      return 1;
    }
  }

  if (set_host && !(set_domain && set_server)) {
    char *dot = index(host, '.');
    if (dot) {
      dot[0] = '\0';
      dot++;
      if (!set_domain) domain = dot;
      if (!set_server) server = host;
    }
  }

  if ((set_port + set_protocol) == 1) {
    struct servent *srv;
    if (set_port && port == 3389) {
      protocol = "rdp";
    } else if (set_protocol) {
      srv = getservbyname(protocol, NULL);
      if (srv) port = ntohs(srv->s_port);
    } else {
      srv = getservbyport(htons(port), NULL);
      if (srv) protocol = srv->s_name;
    }
  }

  if (verbose) {
#define VALUE(X) printf("%s: %s\n", #X, X ? X : "(null)")
    VALUE(user);
    VALUE(domain);
    VALUE(server);
    VALUE(host);
    VALUE(protocol);
#undef VALUE
    printf("port: %d\n", port);
  }

  if (!gnome_keyring_is_available()) {
    fprintf(stderr, "No keyring available\n");
    return 1;
  }

  if (list)
    return key_listing();

  for (tried = 0; tried < 2; tried++) {
    result = gnome_keyring_find_network_password_sync(
      user, domain, server, object, protocol, authtype, port,
      &results
    );
    if (verbose) printf("attempt #%d: ", tried);
    if (result == OK) {
      GList *current;
      GnomeKeyringNetworkPasswordData *passdata;
      char *password;
      for (i = 0, current = results; current; i++, current = current->next) {
        passdata = (GnomeKeyringNetworkPasswordData *)current->data;
        password = passdata->password;
        if (verbose) {
          printf("Result[%d]=%s\n", i, password);
          continue;
        }
        if (remove) {
          result = gnome_keyring_item_delete_sync(
            passdata->keyring,
            passdata->item_id
          );
          if (verbose)
            printf("Remove %s %d -> %s\n", passdata->keyring, passdata->item_id, result == OK ? "OK" : "NOT OK");
          if (!current->next)
            return 0;
          continue;
        }
        printf("%s", password);
        return 0;
      }
      if (password && *password)
        break;
    }
    if (remove) {
      printf("No such password\n");
      return 1;
    }
    if (verbose) printf("nope\n");
    if (!tried) {
      char *password = password_prompt(user, server, domain, protocol, port);
      if (password && *password) {
        guint32 item_id;
        gnome_keyring_set_network_password_sync(
          use_keyring,
          user, domain, server, object, protocol, authtype, port,
          password, &item_id
        );
        if (verbose) printf("Stored password? %s\n", item_id ? "yes" : "no");
      }
    }
  }
  return 0;
}