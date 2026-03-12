#!/bin/bash

# Initialize main variables
GIVEN_PATH=""
GIVEN_COMMAND=""

# Get flag options
while getopts ":p:c:" opt; do
    case $opt in
    p)
        GIVEN_PATH="$OPTARG";;
    c)
        GIVEN_COMMAND="$OPTARG";;
    :)
        echo "Usage: ./fss_script.sh -p <path> -c <command>"
        exit 1;;
    esac
done

# If path or command is empty string terminate
if [ -z $GIVEN_PATH ] || [ -z $GIVEN_COMMAND ]; then
    echo "Usage: ./fss_script.sh -p <path> -c <command>"
    exit 1
fi

# If command is not valid, print valid commands
if [ "$GIVEN_COMMAND" != "listAll" ] && [ "$GIVEN_COMMAND" != "listMonitored" ] && [ "$GIVEN_COMMAND" != "listStopped" ] && [ "$GIVEN_COMMAND" != "purge" ]; then
    echo "Command must be one of theese:"
    echo "listAll, listMonitored, listStopped, purge"
    exit 1
fi

# If path doesn't exist
if [ ! -e $GIVEN_PATH ]; then
    echo "$GIVEN_PATH doesn't exist"
    exit 1
fi

# If command refers to a file, make sure given path is a file
if [ ! -f $GIVEN_PATH ] && [ "$GIVEN_COMMAND" != "purge" ]; then
    echo "Path must be a file";
    exit 1
fi

# List All command
if [ "$GIVEN_COMMAND" == "listAll" ]; then
    grep "\[FULL\]" $GIVEN_PATH | sort -r | sort -t ' ' -k3,3 -u | 
    awk -F '[\\[\\]]' '{
        timestamp = $2
        source_dir = $4
        target_dir = $6
        result = $12
        
        printf "%s -> %s [Last Sync: %s] [%s]\n", source_dir, target_dir, timestamp, result
    }'

    # grep     -> finds all sync reports
    # 1st sort -> sorts entries by reverse order so in the x=next sort we take the one with the newest time 
    # 2nd sort -> sorts entries by source directories and keeps unique source directories 
    # awk      -> finds timestamp, source directory, target directory and result
    #             in each entry and prints it with our format

    exit 0
fi

# List Monitored command
if [ "$GIVEN_COMMAND" == "listMonitored" ]; then
    grep -E "Monitoring .* for" $GIVEN_PATH | sort -r | cut -d ' ' -f 4,6 |
    sort -k2,2 -u | grep "started" | cut -d ' ' -f 2 |
    xargs -I {} grep "\[{}\].*\[FULL\]" "$GIVEN_PATH" | sort -r | sort -t ' ' -k3,3 -u |
    awk -F '[\\[\\]]' '{
        timestamp = $2
        source_dir = $4
        target_dir = $6
        result = $12
        
        printf "%s -> %s [Last Sync: %s] [%s]\n", source_dir, target_dir, timestamp, result
    }'

    # 1st line -> Gets all lines that have "Monitoring started for" or "Monitoring stopped for", 
    #             sorts the in reverse order so that the last stop/start "operation" in a certain
    #             directory is higher and cuts of only the directory and started/stopped parts
    # 2nd line -> From the previous line's result, keep the rows with the unique second fields 
    #             (that is why we needed to sort in reverse), then keep the rows who say started
    #             (last "operation" of ceratin dir was 'started' monitoring) and cut dir name
    # 3rd line -> Get lines of full sync of the directories from previous line, sort them in reverse
    #             order, and keep unique source directories (we find last sync info this way)
    # awk      -> Print in formated way, same logic as listAll

    exit 0
fi

# List Cancelled command
if [ "$GIVEN_COMMAND" == "listStopped" ]; then
    grep -E "Monitoring .* for" $GIVEN_PATH | sort -r | cut -d ' ' -f 4,6 |
    sort -k2,2 -u | grep "stopped" | cut -d ' ' -f 2 |
    xargs -I {} grep "\[{}\].*\[FULL\]" "$GIVEN_PATH" | sort -r | sort -t ' ' -k3,3 -u |
    awk -F '[\\[\\]]' '{
        timestamp = $2
        source_dir = $4
        target_dir = $6
        result = $12
        
        printf "%s -> %s [Last Sync: %s] [%s]\n", source_dir, target_dir, timestamp, result
    }'

    # Exactly the same logic as before, but we want the last "operation"
    # to be 'stopped' instead of 'started'

    exit 0
fi

# Purge command
if [ "$GIVEN_COMMAND" == "purge" ]; then
    echo "Deleting $GIVEN_PATH..."
    rm -rf $GIVEN_PATH
    echo "Purge completed."
    
    exit 0
fi

# if script reached this point something went wrong with the comparissons
exit 1