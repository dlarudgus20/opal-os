#ifndef OPAL_SHELL_SHELL_CMD_H
#define OPAL_SHELL_SHELL_CMD_H

int shell_cmd_priotest(int argc, char **argv);
int shell_cmd_irfdump(int argc, char **argv);
int shell_cmd_rwsec(int argc, char **argv);
int shell_cmd_testrwsec(int argc, char **argv);
int shell_cmd_lsblk(int argc, char **argv);
int shell_cmd_diskreset(int argc, char **argv);
int shell_cmd_diskrescan(int argc, char **argv);
int shell_cmd_lspart(int argc, char **argv);
int shell_cmd_mkpart(int argc, char **argv);
int shell_cmd_rmpart(int argc, char **argv);
int shell_cmd_mount(int argc, char **argv);
int shell_cmd_mkfs(int argc, char **argv);
int shell_cmd_cat(int argc, char **argv);
int shell_cmd_ls(int argc, char **argv);
int shell_cmd_mkdir(int argc, char **argv);

#endif
