#
#	RPM spec file for ${settings.title}
#
Summary: ${settings.title} -- The "C" language web framework
Name: ${settings.name}
Version: ${settings.version}
License: commercial
Group: Development/Other
URL: https://www.embedthis.com/esp
Distribution: Embedthis
Vendor: Embedthis Software
BuildRoot: ${prefixes.rpm}/BUILDROOT/${settings.name}-${settings.version}.${platform.mappedCpu}
AutoReqProv: no

%description
Bit is a replacement for the autoconf/make build tools. Bit is a single tool that configures, builds and generated native build projects for applications. It expresses build rules in the JavaScript language.

%prep

%build

%install
    mkdir -p ${prefixes.rpm}/BUILDROOT/${settings.name}-${settings.version}.${platform.mappedCpu}
    cp -r ${prefixes.content}/* ${prefixes.rpm}/BUILDROOT/${settings.name}-${settings.version}.${platform.mappedCpu}

%clean

%files -f binFiles.txt

%post
if [ -x /usr/bin/chcon ] ; then
	sestatus | grep enabled >/dev/null 2>&1
	if [ $? = 0 ] ; then
		for f in ${prefixes.vapp}/bin/*.so ; do
			chcon /usr/bin/chcon -t texrel_shlib_t $f
		done
	fi
fi
ldconfig -n ${prefixes.vapp}/bin

%preun
rm -f ${prefixes.app}/latest

%postun
