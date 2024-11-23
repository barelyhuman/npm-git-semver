#include <cjson/cJSON.h>
#include <git2.h>
#include <limits.h>
#include <semver/semver.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// defs
#define SLICE_SIZE 50
#define DELIMITER "."
#define PR_DELIMITER "-"
#define MT_DELIMITER "+"

// fn declarations
static void _concat_num(char *str, int x, char *sep);
static void _concat_char(char *str, char *x, char *sep);
static void check_git_error(int error_code, const char *action);
void _semver_render(semver_t *x, char *dest);
char *get_last_commit_hash(git_repository *repo);

// Function to read a file into a string
char *read_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    perror("Unable to open file");
    return NULL;
  }

  // Seek to the end of the file to find its size
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Allocate memory for the entire file content
  char *content = (char *)malloc(length + 1);
  if (content == NULL) {
    perror("Memory allocation failed");
    fclose(file);
    return NULL;
  }

  // Read the file into memory
  fread(content, 1, length, file);
  content[length] = '\0'; // Null-terminate the string

  fclose(file);
  return content;
}

int main() {

  if (git_libgit2_init() < 0) {
    // Handle the initialization error
    fprintf(stderr, "libgit2 initialization failed\n");
    return -1;
  }

  int git_error;

  const char *filename = "package.json";
  char *file_content = read_file(filename);
  if (file_content == NULL) {
    return 1;
  }

  cJSON *json = cJSON_Parse(file_content);
  if (json == NULL) {
    printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
    free(file_content);
    git_libgit2_shutdown();
    return 1;
  }

  cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
  if (!cJSON_IsString(version) || version->valuestring == NULL) {
    return 1;
  }

  char *version_string = version->valuestring;

  semver_t current_version = {};

  if (semver_parse(version_string, &current_version)) {
    fprintf(stderr, "Invalid semver string\n");
    cJSON_Delete(json);
    free(file_content);
    return -1;
  }

  git_repository *repo;
  git_error = git_repository_open(&repo, ".");
  check_git_error(git_error, "Opening repository");

  char *oid = get_last_commit_hash(repo);

  if (oid == NULL) {
    perror("Failed to get hash from the latest git commit");
    git_repository_free(repo);
    git_libgit2_shutdown();
    return 1;
  }

  char prerelease[260];
  snprintf(prerelease, sizeof(prerelease), "pr.%s", oid);
  current_version.prerelease = strdup(prerelease);

  char *pr_release_version = (char *)malloc(sizeof(prerelease));
  _semver_render(&current_version, pr_release_version);

  printf("%s\n",pr_release_version);

  semver_clean(pr_release_version);
  cJSON_Delete(json);
  free(file_content);
  git_repository_free(repo);
  git_libgit2_shutdown();

  return 0;
}

void _semver_render(semver_t *x, char *dest) {
  if (x->major >= 0)
    _concat_num(dest, x->major, NULL);
  if (x->minor >= 0)
    _concat_num(dest, x->minor, DELIMITER);
  if (x->patch >= 0)
    _concat_num(dest, x->patch, DELIMITER);
  if (x->prerelease)
    _concat_char(dest, x->prerelease, PR_DELIMITER);
  if (x->metadata)
    _concat_char(dest, x->metadata, MT_DELIMITER);
}

static void _concat_num(char *str, int x, char *sep) {
  char buf[SLICE_SIZE] = {0};
  if (sep == NULL)
    sprintf(buf, "%d", x);
  else
    sprintf(buf, "%s%d", sep, x);
  strncat(str, buf, SLICE_SIZE - strlen(str) - 1);
}

static void _concat_char(char *str, char *x, char *sep) {
  char buf[SLICE_SIZE] = {0};
  sprintf(buf, "%s%s", sep, x);
  strncat(str, buf, SLICE_SIZE - strlen(str) - 1);
}

char *get_last_commit_hash(git_repository *repo) {
  int rc;
  git_commit *commit = NULL; /* the result */
  git_oid oid_parent_commit; /* the SHA1 for last commit */

  /* resolve HEAD into a SHA1 */
  rc = git_reference_name_to_id(&oid_parent_commit, repo, "HEAD");
  if (rc == 0) {
    /* get the actual commit structure */
    rc = git_commit_lookup(&commit, repo, &oid_parent_commit);
    if (rc == 0) {
      return git_oid_tostr_s(&oid_parent_commit);
    }
  }
  return NULL;
}

static void check_git_error(int error_code, const char *action) {
  const git_error *error = git_error_last();
  if (!error_code)
    return;

  printf("Error %d %s - %s\n", error_code, action,
         (error && error->message) ? error->message : "???");

  exit(1);
}