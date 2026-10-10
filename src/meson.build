kast_sources = files(
  'main.c',
  'util.c',
  'dash.c',
  'procs.c',
  'graphs.c',
  'disks.c',
  'files.c',
)

kast_resource = gnome.compile_resources(
  'kast-resources',
  'kast.gresource.xml',
  c_name: 'kast',
  source_dir: meson.project_source_root(),
)

executable('kast-manager',
  [config_h, kast_sources, kast_resource],
  include_directories: rootInclude,
  dependencies: [gtk4, libadwaita, libgtop, glib, gio, gmodule],
  install: true,
)
