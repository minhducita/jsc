/*
 * dselect - Debian package maintenance user interface
 * method.cc - access method handling
 *
 * Copyright © 1995 Ian Jackson <ian@chiark.greenend.org.uk>
 * Copyright © 2001,2002 Wichert Akkerman <wakkerma@debian.org>
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <compat.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#include <dpkg/i18n.h>
#include <dpkg/dpkg.h>
#include <dpkg/dpkg-db.h>
#include <dpkg/subproc.h>
#include <dpkg/command.h>

#include "dselect.h"
#include "method.h"

static const char *const methoddirectories[]= {
  LIBDIR "/" METHODSDIR,
  LOCALLIBDIR "/" METHODSDIR,
  nullptr
};

static char *methodlockfile = nullptr;
static int methlockfd= -1;

static void
sthfailed(const char * reasoning)
{
  char buf[2048];

  curseson();
  clear();
  sprintf(buf,_("\n\n%s: %s\n"),DSELECT,reasoning);
  addstr(buf);
  attrset(A_BOLD);
  addstr(_("\nPress <enter> to continue."));
  attrset(A_NORMAL);
  refresh(); getch();
}

static void cu_unlockmethod(int, void**) {
  struct flock fl;

  assert(methodlockfile);
  assert(methlockfd);
  fl.l_type=F_UNLCK; fl.l_whence= SEEK_SET; fl.l_start=fl.l_len=0;
  if (fcntl(methlockfd,F_SETLK,&fl) == -1)
    sthfailed("unable to unlock access method area");
}

static enum urqresult ensureoptions(void) {
  const char *const *ccpp;
  dselect_option *newoptions;
  int nread;

  if (!options) {
    newoptions = nullptr;
    nread= 0;
    for (ccpp= methoddirectories; *ccpp; ccpp++)
      readmethods(*ccpp, &newoptions, &nread);
    if (!newoptions) {
      sthfailed("no access methods are available");
      return urqr_fail;
    }
    options= newoptions;
    noptions= nread;
  }
  return urqr_normal;
}

static enum urqresult lockmethod(void) {
  struct flock fl;

  if (methodlockfile == nullptr)
    methodlockfile = dpkg_db_get_path(METHLOCKFILE);

  if (methlockfd == -1) {
    methlockfd= open(methodlockfile, O_RDWR|O_CREAT|O_TRUNC, 0660);
    if (methlockfd == -1) {
      if ((errno == EPERM) || (errno == EACCES)) {
        sthfailed("requested operation requires superuser privilege");
        return urqr_fail;
      }
      sthfailed("unable to open/create access method lockfile");
      return urqr_fail;
    }
  }
  fl.l_type=F_WRLCK; fl.l_whence=SEEK_SET; fl.l_start=fl.l_len=0;
  if (fcntl(methlockfd,F_SETLK,&fl) == -1) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      sthfailed("the access method area is already locked");
      return urqr_fail;
      }
    sthfailed("unable to lock access method area");
    return urqr_fail;
  }
  push_cleanup(cu_unlockmethod, ~0, nullptr, 0, 0);
  return urqr_normal;
}

static urqresult
falliblesubprocess(struct command *cmd)
{
  pid_t pid;
  int status, i, c;

  cursesoff();

  subproc_signals_setup(cmd->name);

  pid = subproc_fork();
  if (pid == 0) {
    subproc_signals_cleanup(0, nullptr);
    command_exec(cmd);
  }

  status = subproc_wait(pid, cmd->name);

  pop_cleanup(ehflag_normaltidy);

  fprintf(stderr, "\n");
  i = subproc_check(status, cmd->name, PROCWARN);
  if (i == 0) {
    sleep(1);
    return urqr_normal;
  }
  fprintf(stderr,_("Press <enter> to continue.\n"));
  m_output(stderr, _("<standard error>"));
  do { c= fgetc(stdin); } while ((c == ERR && errno==EINTR) || ((c != '\n') && c != EOF));
  if ((c == ERR) || (c == EOF))
    ohshite(_("error reading acknowledgement of program failure message"));
  return urqr_fail;
}

static urqresult runscript(const char *exepath, const char *name) {
  urqresult ur;

  ur= ensureoptions();  if (ur != urqr_normal) return ur;
  ur=lockmethod();  if (ur != urqr_normal) return ur;
  getcurrentopt();

  if (coption) {
    struct command cmd;

    strcpy(coption->meth->pathinmeth,exepath);

    command_init(&cmd, coption->meth->path, name);
    command_add_args(&cmd, exepath, dpkg_db_get_dir(),
                     coption->meth->name, coption->name, nullptr);
    ur = falliblesubprocess(&cmd);
    command_destroy(&cmd);
  } else {
    sthfailed("no access method is selected/configured");
    ur= urqr_fail;
  }
  pop_cleanup(ehflag_normaltidy);

  return ur;
}

urqresult urq_update(void) {
  return runscript(METHODUPDATESCRIPT,_("update available list script"));
}

urqresult urq_install(void) {
  return runscript(METHODINSTALLSCRIPT,_("installation script"));
}

static urqresult rundpkgauto(const char *name, const char *dpkgmode) {
  urqresult ur;
  struct command cmd;

  command_init(&cmd, DPKG, name);
  command_add_args(&cmd, DPKG, "--admindir", dpkg_db_get_dir(), "--pending",
                   dpkgmode, nullptr);

  cursesoff();
  printf("running dpkg --pending %s ...\n",dpkgmode);
  fflush(stdout);
  ur = falliblesubprocess(&cmd);
  command_destroy(&cmd);

  return ur;
}

urqresult urq_remove(void) {
  return rundpkgauto("dpkg --remove","--remove");
}

urqresult urq_config(void) {
  return rundpkgauto("dpkg --configure","--configure");
}

urqresult urq_setup(void) {
  quitaction qa;
  urqresult ur;

  ur= ensureoptions();  if (ur != urqr_normal) return ur;
  ur=lockmethod();  if (ur != urqr_normal) return ur;
  getcurrentopt();

  curseson();
  methodlist *l= new methodlist();
  qa= l->display();
  delete l;

  if (qa == qa_quitchecksave) {
    struct command cmd;

    strcpy(coption->meth->pathinmeth,METHODSETUPSCRIPT);

    command_init(&cmd, coption->meth->path, _("query/setup script"));
    command_add_args(&cmd, METHODSETUPSCRIPT, dpkg_db_get_dir(),
                     coption->meth->name, coption->name, nullptr);
    ur = falliblesubprocess(&cmd);
    command_destroy(&cmd);
    if (ur == urqr_normal) writecurrentopt();
  } else {
    ur= urqr_fail;
  }

  pop_cleanup(ehflag_normaltidy);
  return ur;
}
