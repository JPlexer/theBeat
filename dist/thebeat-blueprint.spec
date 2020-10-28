Name:           theBeat-blueprint
Version:        3.0.0
Release:        1%{?dist}
Summary:        Music Player

License:        GPLv3+
URL:            https://github.com/vicr123/thebeat
Source0:        https://github.com/vicr123/theBeat/archive/v%{version}.tar.gz

%if 0%{?fedora} == 32
BuildRequires:  make qt5-devel qt5-qtmultimedia-devel the-libs-blueprint-devel phonon-qt5-devel taglib-devel
Requires:       qt5 qt5-qtmultimedia the-libs-blueprint phonon-qt5 taglib
%endif

%if 0%{?fedora} >= 33
BuildRequires:  make qt5-qtbase-devel qt5-qtmultimedia-devel the-libs-blueprint-devel phonon-qt5-devel taglib-devel qt5-linguist
Requires:       qt5-qtbase qt5-qtmultimedia the-libs-blueprint phonon-qt5 taglib
%endif

%define debug_package %{nil}
%define _unpackaged_files_terminate_build 0

%description
Music Player

%prep
%setup

%build
qmake-qt5
make

%install
rm -rf $RPM_BUILD_ROOT
#%make_install
make install INSTALL_ROOT=$RPM_BUILD_ROOT
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%{_bindir}/thebeat
%{_datadir}/applications/thebeat.desktop
%{_datadir}/icons/thebeat.svg


%changelog
* Sun May  3 2020 Victor Tran
- 
