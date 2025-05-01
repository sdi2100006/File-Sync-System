#!/bin/sh
# This is a comment!

while getopts p:c: flag
do
    case $flag in
        p) PATH_NAME=$OPTARG;;

        c) COMMAND=$OPTARG;;
    esac
done
echo "path: $PATH_NAME";
echo "command: $COMMAND";

case $COMMAND in
    purge)  if [ -e "$PATH_NAME" ]; 
                then if [ -f "$PATH_NAME" ];
                    then echo " would run rm -f $PATH_NAME"
                    #rm -f "$PATH_NAME"
                elif [ -d "$PATH_NAME" ];
                    then echo "would run rm -rf $PATH_NAME"
                    #rm -rf "$PATH_NAME"
                fi
            else
                echo "File or directory doesnt exist"
            fi
            ;;
esac