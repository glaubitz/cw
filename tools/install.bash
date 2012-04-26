#############################################################################
#############################################################################
#
# install.bash
#
#############################################################################
#############################################################################





#############################################################################
# copy_file
#############################################################################
copy_file()
	{
	[ -e "$1" ] || return
	echo "copying '$1' -> '$2'" &&
	chown root:root "$1" &&
	mkdir -p "${2%/*}" &&
	cp -a "$1" "$2" || exit 1
	}



#############################################################################
# upgrade_file
#############################################################################
upgrade_file()
	{
	[ -e "$1" ] || return
	if [ ! -e "$2" ]; then
		echo "file '$2' not found, no upgrade possible" 1>&2
		exit 1
	fi
	echo "upgrading '$1' -> '$2'" &&
	chown root:root "$1" &&
	cp -a "$1" "$2" || exit 1
	}



#############################################################################
# remove_file
#############################################################################
remove_file()
	{
	[ -e "$1" ] || return
	echo "removing '$1'" &&
	rm -f "$1" || exit 1
	}



#############################################################################
# write_uninstall_info
#############################################################################
write_uninstall_info()
	{
	cat <<-EOC > .uninstall
	MAJOR="$MAJOR"
	BIN_DIR="$BIN_DIR"
	MAN_DIR="$MAN_DIR"
	MODULE_DIR="$MODULE_DIR"
	MODULE_NAME="$MODULE_NAME"
	MODULES_CONF="$MODULES_CONF"
	UDEV_STATIC_DEVICES="$UDEV_STATIC_DEVICES"
	EOC
	}



#############################################################################
# check_char_major
#############################################################################
check_char_major()
	{
	awk -v major="$MAJOR" '/^Character devices:$/ {
		s = 0;
		}

		{
		if (s == 1) next;
		}

	/^$/ {
		s = 1;
		}

	(s == 0) && ($1 == major) {
		q = "'\''";
		print >"/dev/stderr";
		printf("found the above line in %s/proc/devices%s, which\n", q, q) >"/dev/stderr";
		printf("indicates that char major %s is already in use!\n", major) >"/dev/stderr";
		exit(1);
		}' /proc/devices || exit 1
	}



#############################################################################
# modify_modules_conf
#############################################################################
modify_modules_conf()
	{
	if [ ! -e "$MODULES_CONF" ]; then
		echo "file '$MODULES_CONF' not found" 1>&2
		return
	fi
	LINES="$(awk -v file="$MODULES_CONF" '{
		line++;
		}

	/^options '"$MODULE_NAME"' cw_major='"$MAJOR"'$/ ||
	/^alias char-major-'"$MAJOR $MODULE_NAME"'$/ {
		printf("%8d %s\n", line, $0);
		next;
		}

	/^[ \t]*(alias[ \t]+char-major-[0-9]+|options)[ \t]+'"$MODULE_NAME"'([ \t]+|$)/ ||
	/^[ \t]*alias[ \t]+char-major-'"$MAJOR"'/ {
		i++;
		printf("%8d %s\n", line, $0) >"/dev/stderr";
		next;
		}

	END {
		if (i == 0) exit (0);
		q = "'\''";
		printf("found the above lines in %s%s%s,\n", q, file, q) >"/dev/stderr";
		printf("you need to remove them first!\n") >"/dev/stderr";
		exit(1);
		}' "$MODULES_CONF")" || exit 1
	if [ "$LINES" ]; then 
		echo "needed entries already in '$MODULES_CONF'"
		echo "$LINES"
	else
		echo "adding entries to '$MODULES_CONF'"
		{
		echo "alias char-major-$MAJOR $MODULE_NAME"
		echo "options $MODULE_NAME cw_major=$MAJOR"
		} >> "$MODULES_CONF"
	fi
	}



#############################################################################
# cleanup_modules_conf
#############################################################################
cleanup_modules_conf()
	{
	if [ ! -e "$MODULES_CONF" ]; then
		echo "file '$MODULES_CONF' not found" 1>&2
		return
	fi
	echo "cleaning up '$MODULES_CONF'"
	awk '! /^alias char-major-'"$MAJOR $MODULE_NAME"'$/ && ! /^options '"$MODULE_NAME"' cw_major='"$MAJOR"'$/'  \
		"$MODULES_CONF" > "$MODULES_CONF.tmp" &&
	cat "$MODULES_CONF.tmp" > "$MODULES_CONF" &&
	rm -f "$MODULES_CONF.tmp" || exit 1
	}



#############################################################################
# modify_modprobe_d
#############################################################################
modify_modprobe_d()
	{
	if [ ! -d "$MODULES_CONF" ]; then
		echo "directory '$MODULES_CONF' not found" 1>&2
		return
	fi
	if [ -e "$MODULES_CONF/catweasel" ]; then
		echo "file '$MODULES_CONF/catweasel' already exists" 1>&2
		exit 1
	fi
	echo "adding entries to '$MODULES_CONF/catweasel'"
	{
	echo "alias char-major-$MAJOR $MODULE_NAME"
	echo "options $MODULE_NAME cw_major=$MAJOR"
	} > "$MODULES_CONF/catweasel"
	}



#############################################################################
# cleanup_modprobe_d
#############################################################################
cleanup_modprobe_d()
	{
	if [ ! -d "$MODULES_CONF" ]; then
		echo "directory '$MODULES_CONF' not found"
		return
	fi
	rm -f "$MODULES_CONF/catweasel"
	}



#############################################################################
# modify_module_autoload
#############################################################################
modify_module_autoload()
	{
	if [ -d "$MODULES_CONF" ]; then
		modify_modprobe_d
	else
		modify_modules_conf
	fi
	echo "executing depmod" &&
	depmod -q -a || exit 1
	}



#############################################################################
# cleanup_module_autoload
#############################################################################
cleanup_module_autoload()
	{
	if [ -d "$MODULES_CONF" ]; then
		cleanup_modprobe_d
	else
		cleanup_modules_conf
	fi
	}



#############################################################################
# unload_module
#############################################################################
unload_module()
	{

	# Linux 2.4 lists the module with its file name, while Linux 2.6
	# lists it with its internal name, to handle both cases rmmod
	# is called with both names

	echo "removing kernel module (if loaded)" &&
	rmmod cw "$MODULE_NAME" 2>/dev/null
	echo "executing depmod" &&
	depmod -q -a || exit 1
	}



#############################################################################
# create_device_nodes
#############################################################################
create_device_nodes()
	{
	DIR="$1"
	[ -d "$DIR" ] || return
	for CONTROLLER in 0 1 2 3; do
		for FLOPPY in 0 1; do
			DEVICE="$DIR"'/cw'"$CONTROLLER"'raw'"$FLOPPY" &&
			MINOR="$(($CONTROLLER * 64 + $FLOPPY * 32 + 31))" &&
			if [ -e "$DEVICE" ]; then
				echo "'$DEVICE' already exists"
				read MAJOR_TMP MINOR_TMP < <(env -i ls -ln "$DEVICE" | awk '{ sub(/,$/, "", $5); print($5, $6); }')
				[ "$MAJOR_TMP" = "$MAJOR" -a "$MINOR_TMP" = "$MINOR" ] && continue
				echo "'$DEVICE' has different major or minor number, you need to fix that" 1>&2
				exit 1
			else
				echo "creating '$DEVICE'"
				mknod "$DEVICE" c "$MAJOR" "$MINOR" || exit 1
			fi
		done
	done
	}



#############################################################################
# remove_device_nodes
#############################################################################
remove_device_nodes()
	{
	DIR="$1"
	[ -d "$DIR" ] || return
	for CONTROLLER in 0 1 2 3; do
		for FLOPPY in 0 1; do
			remove_file "$DIR"'/cw'"$CONTROLLER"'raw'"$FLOPPY"
		done
	done
	}



#############################################################################
# modify_static_devices
#############################################################################
modify_static_devices()
	{

	# seems only SuSE 10.0 uses this

	[ -e "$UDEV_STATIC_DEVICES" ] || return
	for CONTROLLER in 0 1 2 3; do
		for FLOPPY in 0 1; do
			NAME='cw'"$CONTROLLER"'raw'"$FLOPPY"
			MINOR="$(($CONTROLLER * 64 + $FLOPPY * 32 + 31))"
			LINE="$NAME c $MAJOR $MINOR 644"
			[ "$(grep '^'"$LINE"'$' "$UDEV_STATIC_DEVICES")" ] && continue
			awk -v file="$UDEV_STATIC_DEVICES" '{
				line++;
				}

			/^'"$NAME"' / {
				i++
				printf("%8d %s\n", line, $0) >"/dev/stderr";
				next;
				}

			END {
				if (i == 0) exit (0);
				q = "'\''";
				printf("found the above lines in %s%s%s,\n", q, file, q) >"/dev/stderr";
				printf("you need to remove them first!\n") >"/dev/stderr";
				exit(1);
				}' "$UDEV_STATIC_DEVICES" || exit 1
			echo "adding entry '$NAME' to '$UDEV_STATIC_DEVICES'"
			echo "$LINE" >> "$UDEV_STATIC_DEVICES"
		done
	done
	}



#############################################################################
# cleanup_static_devices
#############################################################################
cleanup_static_devices()
	{

	# seems only SuSE 10.0 uses this

	[ -e "$UDEV_STATIC_DEVICES" ] || return
	echo "cleaning up '$UDEV_STATIC_DEVICES'"
	awk '! /^cw[0-3]raw[01] /' "$UDEV_STATIC_DEVICES" > "$UDEV_STATIC_DEVICES.tmp" &&
	cat "$UDEV_STATIC_DEVICES.tmp" > "$UDEV_STATIC_DEVICES" &&
	rm -f "$UDEV_STATIC_DEVICES.tmp" || exit 1
	}



#############################################################################
# main
#############################################################################
[ "$MAJOR" ]               || MAJOR="120"
[ "$BIN_DIR" ]             || BIN_DIR="/usr/bin"
[ "$MAN_DIR" ]             || MAN_DIR="/usr/share/man"
[ "$MODULE_DIR" ]          || MODULE_DIR="/lib/modules/$(uname -r)/kernel/drivers/char"
[ "$MODULE_NAME" ]         || MODULE_NAME="cw"
[ "$UDEV_STATIC_DEVICES" ] || UDEV_STATIC_DEVICES="/etc/udev/static_devices.txt"
if [ -z "$MODULES_CONF" ]; then
	VERSION="$(uname -r | awk 'BEGIN { FS="."; } { print($1 $2); }')"
	MODULES_CONF="/etc/modules.conf"
	if [ "$VERSION" -ge "26" ]; then
		MODULES_CONF="/etc/modprobe.conf"
		[ -e "$MODULES_CONF" ] || MODULES_CONF="/etc/modprobe.d"
	fi
fi
MAN_DIR="$MAN_DIR/man1"

PROGRAM_NAME="$(basename $0)"
if [ "$PROGRAM_NAME" = "install.bash" ]; then
	check_char_major
	write_uninstall_info
	copy_file "bin/cwtool" "$BIN_DIR/cwtool"
	copy_file "module/cw.o" "$MODULE_DIR/$MODULE_NAME.o"
	copy_file "module/cw.ko" "$MODULE_DIR/$MODULE_NAME.ko"
	copy_file "doc/cwtool.1" "$MAN_DIR/cwtool.1"
	modify_module_autoload
	create_device_nodes "/dev"
	create_device_nodes "/lib/udev/devices"
	create_device_nodes "/etc/udev/devices"
	modify_static_devices
elif [ "$PROGRAM_NAME" = "upgrade.bash" ]; then
	write_uninstall_info
	upgrade_file "bin/cwtool" "$BIN_DIR/cwtool"
	upgrade_file "module/cw.o" "$MODULE_DIR/$MODULE_NAME.o"
	upgrade_file "module/cw.ko" "$MODULE_DIR/$MODULE_NAME.ko"
	upgrade_file "doc/cwtool.1" "$MAN_DIR/cwtool.1"
	unload_module
	create_device_nodes "/dev"
	create_device_nodes "/lib/udev/devices"
	create_device_nodes "/etc/udev/devices"
	modify_static_devices
elif [ "$PROGRAM_NAME" = "uninstall.bash" ]; then
	[ -f .uninstall ] && . .uninstall
	remove_file "$BIN_DIR/cwtool"
	remove_file "$MODULE_DIR/$MODULE_NAME.o"
	remove_file "$MODULE_DIR/$MODULE_NAME.ko"
	remove_file "$MAN_DIR/cwtool.1"
	cleanup_module_autoload
	unload_module
	remove_device_nodes "/dev"
	remove_device_nodes "/lib/udev/devices"
	remove_device_nodes "/etc/udev/devices"
	cleanup_static_devices
else
	echo "unknown program name '$PROGRAM_NAME'" 2>&1
fi
exit 0
######################################################### Karsten Scheibler #
