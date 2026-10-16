kast_sources = files(
  'main.c',
  'util.c',
  'dash.c',
  'procs.c',
  'graphs.c',
  'disks.c',
  'files.c',
  'rating.c',
  'controls.c',
  'widget.c',
  'sysinfo.c',
  'extra.c',
  'extra2.c',
)

math = cc.find_library('m', required: true)

kast_resource = gnome.compile_resources(
  'kast-resources',
  'kast.gresource.xml',
  c_name: 'kast',
  source_dir: meson.project_source_root(),
)

executable('kast-manager',
  [config_h, kast_sources, kast_resource],
  include_directories: rootInclude,
  dependencies: [gtk4, libadwaita, libgtop, glib, gio, gmodule, math],
  install: true,
)

install_data('kast-tui',
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x',
)
