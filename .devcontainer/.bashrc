#!/bin/bash

export PROMPT_COMMAND='history -a' && export HISTFILE=/workspace/.bash_history

########################################
# General shortcuts
########################################
alias sb="source ~/.bashrc"
alias gs="git status"
alias gb="git branch"
alias gc="git checkout"
alias gf="git fetch"
alias gcan="git commit --amend --no-edit"
alias gpf="git push --force"

git_branch_name() {
    branch_name="$(git symbolic-ref HEAD 2>/dev/null)" || branch_name="(unnamed branch)"
    echo ${branch_name##refs/heads/}
}

gpup() {
    git push -u origin "$(git_branch_name)"
}

gmain() {
    git fetch origin main:main && git checkout main && git branch -d "$(git_branch_name)"
}

source /usr/share/bash-completion/completions/git

if [ "`id -u`" -eq 0 ]; then
    PS1="\[\033[m\]\[\033[1;35m\]\t\[\033[m\]|\[\e[1;31m\]\u\[\e[1;36m\]\[\033[m\]@\[\e[1;36m\]\h\[\033[m\]\[\e[0m\]\[\e[1;32m\][\W]> \[\e[0m\]"
else
    PS1="\[\033[m\]\[\033[1;35m\]\t\[\033[m\]|\[\e[1m\]\u\[\e[1;36m\]\[\033[m\]@\[\e[1;36m\]\h\[\033[m\]\[\e[0m\]|\e[1;32m\]\w\n$ \[\e[0m\]"
fi

sudo chmod 666 /var/run/docker.sock