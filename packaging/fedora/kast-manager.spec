Name:           kast-manager
Version:        2.8.0
Release:        1%{?dist}
Summary:        Processes, resources, disks and files in one dark place
License:        GPL-3.0-or-later
URL:            https://github.com/tejaskhanna989/kastmanager
Source0:        https://github.com/tejaskhanna989/kastmanager/archive/refs/tags/v%{version}.tar.gz

BuildRequires:  meson
BuildRequires:  ninja-build
BuildRequires:  gcc
BuildRequires:  pkgconfig(gtk4)
BuildRequires:  pkgconfig(libadwaita-1)
BuildRequires:  pkgconfig(libgtop-2.0)
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib

Requires:       gtk4%{?_isa}
Requires:       libadwaita%{?_isa}
Requires:       libgtop2%{?_isa}

%description
Kast Manager 2.x by Tejas Khanna is a fresh minimal system companion
with a dark restrained interface: live dashboard, sortable processes
with End/Kill, hand-drawn graphs, file systems, file browser,
PC rating, full controls, and 20+ more tools.

%prep
%autosetup -n kastmanager-%{version}

%build
%meson --buildtype=release
%meson_build

%install
%meson_install
desktop-file-validate %{buildroot}%{_datadir}/applications/*.desktop || :

%files
%{_bindir}/kast-manager
%{_datadir}/applications/io.github.tejaskhanna989.KastManager.desktop
%{_datadir}/metainfo/io.github.tejaskhanna989.KastManager.metainfo.xml
%{_datadir}/icons/hicolor/

%changelog
* Wed Oct 14 2026 Tejas Khanna <khannatejas989@gmail.com> - 2.8.0-1
- Android companion app.
* Wed Oct 14 2026 Tejas Khanna <khannatejas989@gmail.com> - 2.7.4-1
- Redrawn nav icons.
* Tue Oct 13 2026 Tejas Khanna <khannatejas989@gmail.com> - 2.7.3-1
- Rename symbolic icons to avoid file conflicts.
* Tue Oct 13 2026 Tejas Khanna <khannatejas989@gmail.com> - 2.7.2-1
- Donate region popup.
* Tue Oct 13 2026 Tejas Khanna <khannatejas989@gmail.com> - 2.7.1-1
- Light mode and theme switcher.
* Tue Oct 13 2026 Tejas Khanna <khannatejas989@gmail.com> - 2.7.0-1
* Tue Oct 13 2026 Tejas Khanna <khannatejas989@gmail.com> - 2.6.0-1
- Fresh 2.x upstream release.