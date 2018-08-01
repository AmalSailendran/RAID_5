#!/bin/bash
# prints the input
function raidimport() 
{
  echo 'srcfile: ' $1
  echo 'raidfile: ' $2

  # raidimport srcfile raidfile:
  ./raid5_soft $1 $2 1
}

function raidexport() 
{
  echo 'raidfile,: ' $1
  echo 'dstfile:: ' $2

  # raidexport raidfile, dstfile
  ./raid5_soft $1 $2 2
}

