#!/bin/bash

while [ 1 ]; do
  if [ "$1" != "" ]; then
    cmd="$1";
    shift;
  else
    read -p "grep for what? << " -e cmd;
  fi;
  case "$cmd" in
    "exit")
      break;;
    "")
      ;;
    *)
      query="$cmd";;
  esac
  if [ "$query" == "" ]; then
    continue;
  elif [ -f "$query" ]; then
    less -R "$query";
  else
    clear;
    grep "$query" --color=always log*.txt | sort -gsk2 | less -R;
  fi
done
