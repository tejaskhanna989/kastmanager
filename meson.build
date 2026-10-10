# https://github.com/tejaskhanna989/kastmanager/issues
project('kast-manager',
  'c',
  default_options : [
    'c_std=gnu2x',
    'warning_level=2',
    'b_lto=true',
  ],
  version: '2.0.0',
  meson_version: '>=0.57.0',
)

gnome = import('gnome')

cc = meson.get_compiler('c')

app_id = 'io.github.tejaskhanna989.KastManager'

conf = configuration_data()
conf.set_quoted('APP_ID', app_id)
conf.set_quoted('VERSION', meson.project_version())

dataconf = configuration_data()
dataconf.set('APP_ID', app_id)
dataconf.set('VERSION', meson.project_version())
dataconf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))

gtk4      = dependency('gtk4')
libadwaita = dependency('libadwaita-1')
libgtop   = dependency('libgtop-2.0', version: '>=2.41.2')
glib      = dependency('glib-2.0')
gio       = dependency('gio-2.0')
gmodule   = dependency('gmodule-2.0')

config_h = configure_file(
  output: 'config.h',
  configuration: conf,
)

rootInclude = include_directories('.')

subdir('src')
subdir('data')

gnome.post_install(
  gtk_update_icon_cache: true,
)
