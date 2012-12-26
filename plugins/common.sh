#!/bin/bash

uninstall(){
	del_file=$1
	bak_file=$1"~"
	rm -f $del_file
	if [ -e "$bak_file" ]; then
		echo "recover mode:\""$bak_file"\" ---> \""$del_file"\""
		mv -f $bak_file $del_file
	fi
	
}
