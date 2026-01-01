
// --- TRAVERSALS ---
#include <string.h>

static void str_append(char **buf, size_t *cap, size_t *len, int val) {
  char temp[32];
  snprintf(temp, 32, "%d ", val);
  size_t l = strlen(temp);

  if (*len + l + 1 >= *cap) {
    *cap = (*cap) * 2 + 64;
    *buf = realloc(*buf, *cap);
  }
  strcpy(*buf + *len, temp);
  *len += l;
}

// Recursive Helpers
static void bst_pre_rec(TreeNode *root, char **buf, size_t *cap, size_t *len) {
  if (!root)
    return;
  str_append(buf, cap, len, root->value);
  bst_pre_rec(root->left, buf, cap, len);
  bst_pre_rec(root->right, buf, cap, len);
}

static void bst_in_rec(TreeNode *root, char **buf, size_t *cap, size_t *len) {
  if (!root)
    return;
  bst_in_rec(root->left, buf, cap, len);
  str_append(buf, cap, len, root->value);
  bst_in_rec(root->right, buf, cap, len);
}

static void bst_post_rec(TreeNode *root, char **buf, size_t *cap, size_t *len) {
  if (!root)
    return;
  bst_post_rec(root->left, buf, cap, len);
  bst_post_rec(root->right, buf, cap, len);
  str_append(buf, cap, len, root->value);
}

// Public Wrappers BST
char *bst_preorder(TreeNode *root) {
  size_t cap = 128, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  bst_pre_rec(root, &buf, &cap, &len);
  return buf;
}
char *bst_inorder(TreeNode *root) {
  size_t cap = 128, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  bst_in_rec(root, &buf, &cap, &len);
  return buf;
}
char *bst_postorder(TreeNode *root) {
  size_t cap = 128, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  bst_post_rec(root, &buf, &cap, &len);
  return buf;
}
char *bst_bfs(TreeNode *root) {
  size_t cap = 128, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  if (!root)
    return buf;

  TreeNode **queue = malloc(1000 * sizeof(TreeNode *));
  int front = 0, rear = 0;
  queue[rear++] = root;

  while (front < rear) {
    TreeNode *curr = queue[front++];
    str_append(&buf, &cap, &len, curr->value);
    if (curr->left)
      queue[rear++] = curr->left;
    if (curr->right)
      queue[rear++] = curr->right;
  }
  free(queue);
  return buf;
}

// N-ary Helpers
static void nary_pre_rec(NaryNode *root, char **buf, size_t *cap, size_t *len) {
  if (!root)
    return;
  str_append(buf, cap, len, root->value);
  for (size_t i = 0; i < root->child_count; i++)
    nary_pre_rec(root->children[i], buf, cap, len);
}

static void nary_post_rec(NaryNode *root, char **buf, size_t *cap,
                          size_t *len) {
  if (!root)
    return;
  for (size_t i = 0; i < root->child_count; i++)
    nary_post_rec(root->children[i], buf, cap, len);
  str_append(buf, cap, len, root->value);
}

// Public Wrappers N-ary
char *nary_preorder(NaryNode *root) {
  size_t cap = 128, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  nary_pre_rec(root, &buf, &cap, &len);
  return buf;
}
char *nary_postorder(NaryNode *root) {
  size_t cap = 128, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  nary_post_rec(root, &buf, &cap, &len);
  return buf;
}
char *nary_bfs(NaryNode *root) {
  size_t cap = 128, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  if (!root)
    return buf;

  NaryNode **queue = malloc(1000 * sizeof(NaryNode *));
  int front = 0, rear = 0;
  queue[rear++] = root;

  while (front < rear) {
    NaryNode *curr = queue[front++];
    str_append(&buf, &cap, &len, curr->value);
    for (size_t i = 0; i < curr->child_count; i++)
      queue[rear++] = curr->children[i];
  }
  free(queue);
  return buf;
}
