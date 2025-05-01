#!/bin/sh
while getopts p:c: flag
do
    case $flag in
        p) PATH_NAME=$OPTARG;;

        c) COMMAND=$OPTARG;;

        *) echo "Wrong call"
            exit 1
            ;;
    esac
done

if [ -z "$PATH_NAME" ] || [ -z "$COMMAND" ];
    then echo "One or more aguments missing"
    exit 1
fi

case $COMMAND in
    listAll) echo "I havent done that :( ";;

    listMonitored) echo "I havent done that :( ";;

    listStoped) echo "I havent done that :( ";;

    purge)  if [ -e "$PATH_NAME" ]; 
                then if [ -f "$PATH_NAME" ];
                    then echo "Deleting $PATH_NAME"
                    rm -f "$PATH_NAME"
                    echo "Purge coplete."
                elif [ -d "$PATH_NAME" ];
                    then echo "Deleting $PATH_NAME"
                    rm -rf "$PATH_NAME"
                    echo "Purge complete."
                fi
            else
                echo "File or directory doesnt exist"
            fi
            ;;
    *) echo "Command doesnt exist";;
esac