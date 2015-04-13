Name:       nemo-qml-plugin-calendar-qt5

Summary:    Calendar plugin for Nemo Mobile
Version:    0.1.18
Release:    1
Group:      System/Libraries
License:    BSD
URL:        https://github.com/nemomobile/nemo-qml-plugin-calendar
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(libmkcal-qt5)
BuildRequires:  pkgconfig(libkcalcoren-qt5)
BuildRequires:  pkgconfig(libical)
BuildRequires:  pkgconfig(accounts-qt5)

%description
%{summary}.

%package tests
Summary:    QML calendar plugin tests
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5Test)
Requires:   %{name} = %{version}-%{release}

%description tests
%{summary}.

%package lightweight
Summary:    Calendar lightweight QML plugin
Group:      System/Libraries
BuildRequires:  pkgconfig(Qt5DBus)

%description lightweight
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%qmake_install

%files
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/org/nemomobile/calendar/libnemocalendar.so
%{_libdir}/qt5/qml/org/nemomobile/calendar/qmldir

%files tests
%defattr(-,root,root,-)
/opt/tests/nemo-qml-plugins-qt5/calendar/*

%files lightweight
%defattr(-,root,root,-)
%attr(2755, root, privileged) %{_bindir}/calendardataservice
%{_datadir}/dbus-1/services/org.nemomobile.calendardataservice.service
%{_libdir}/qt5/qml/org/nemomobile/calendar/lightweight/libnemocalendarwidget.so
%{_libdir}/qt5/qml/org/nemomobile/calendar/lightweight/qmldir
