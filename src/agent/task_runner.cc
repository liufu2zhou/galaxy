/*
 * executor.cc
 * Copyright (C) 2015 wangtaize <wangtaize@baidu.com>
 *
 * Distributed under terms of the MIT license.
 */
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include "agent/task_runner.h"
#include "common/logging.h"
#include "common/util.h"
namespace galaxy{
namespace agent{


//check process with m_child_pid
int CommandTaskRunner::IsRunning(){
    if(m_child_pid == -1){
        return -1;
    }
    int ret = ::kill(m_child_pid,0);
    LOG(INFO,"check task %d ret %d",m_task_info.task_name(),ret);
    return ret;
}

//start process
//1. fork a subprocess A
//2. exec command in the subprocess A
//TODO add workspace
int CommandTaskRunner::Start(){
    LOG(INFO,"start a task with id %d",m_task_info.task_name());
    if (m_child_pid != -1){
        LOG(WARNING,"task with id %d has existed",m_task_info.task_name());
        return -1;
    }
    m_mutex->Lock("start task lock");
    if (m_child_pid != -1){
        m_mutex->Unlock();
        return -1;
    }
    m_child_pid = fork();
    //child
    if(m_child_pid == 0){
        pid_t my_pid = getpid();
        int ret = setpgid(my_pid,my_pid);
        if(ret != 0 ){
            return ret;
        }
        pid_t pgid = getpgid(my_pid);
        LOG(INFO,"start task in process %d , group %d",my_pid,pgid);
        RunInnerChildProcess(m_workspace.GetPath(),m_task_info.cmd_line());
    }else{
        m_mutex->Unlock();
        m_group_pid = m_child_pid;
        return 0;
    }
}


int CommandTaskRunner::Stop(){
    if(IsRunning()!=0){
        return 0;
    }
    int ret = killpg(m_group_pid,9);
    return ret;
}

void CommandTaskRunner::RunInnerChildProcess(const std::string &root_path,
                                            const std::string &cmd_line){
    std::string task_stdout = root_path + "./stdout";
    std::string task_stderr = root_path + "./stderr";
    int stdout_fd = open(task_stdout.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);
    int stderr_fd = open(task_stderr.c_str(), O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);
    dup2(stdout_fd, STDOUT_FILENO);
    dup2(stderr_fd, STDERR_FILENO);
    common::util::CloseOnExec(stdout_fd);
    common::util::CloseOnExec(stderr_fd);
    int pid = getpid();
    std::vector<int> fds;
    common::util::GetProcessFdList(pid, fds);
    for (size_t i = 3; i < fds.size(); i++) {
        common::util::CloseOnExec(fds[i]);
    }
    chdir(root_path.c_str());
    LOG(INFO, "RunInnerChildProcess task %s", cmd_line.c_str());
    int ret = execl("/bin/sh", "sh", "-c", cmd_line.c_str(), NULL);
    if (ret != 0) {
        LOG(INFO, "exec failed %d %s", errno, strerror(errno));
    }
}

}
}



