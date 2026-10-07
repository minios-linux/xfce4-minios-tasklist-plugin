project('xfce4-minios-tasklist-plugin', 'c', version: '1.0.0', meson_version: '>= 0.61')

gnome = import('gnome')
i18n = import('i18n')

prefix = get_option('prefix')
libdir = get_option('libdir')
datadir = get_option('datadir')
localedir = prefix / get_option('localedir')
plugin_dir = prefix / libdir / 'xfce4/panel/plugins'
desktop_dir = prefix / datadir / 'xfce4/panel/plugins'

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', 'xfce4-panel')
conf.set_quoted('MINIOS_GETTEXT_PACKAGE', 'xfce4-minios-tasklist-plugin')
conf.set_quoted('LOCALEDIR', localedir)
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set('ENABLE_X11', 1)
conf.set('HAVE_MATH_H', 1)
configure_file(output: 'config.h', configuration: conf)

cc = meson.get_compiler('c')

gtk = dependency('gtk+-3.0')
gio_unix = dependency('gio-unix-2.0')
exo = dependency('exo-2')
xfconf = dependency('libxfconf-0')
xfceutil = dependency('libxfce4util-1.0')
xfceui = dependency('libxfce4ui-2')
xfcepanel = dependency('libxfce4panel-2.0')
xfcewindowing = dependency('libxfce4windowing-0')
xfcewindowingui = dependency('libxfce4windowingui-0')
x11 = dependency('x11')
xext = dependency('xext')

resources = gnome.compile_resources(
  'minios-tasklist-resources',
  'resources/resources.gresource.xml',
  source_dir: 'resources',
  c_name: 'minios_tasklist_resources',
)

sources = files(
  'tasklist.c',
  'tasklist-widget.c',
  'common/panel-utils.c',
  'common/panel-xfconf.c',
  'common/panel-debug.c',
)

shared_module(
  'minios-tasklist',
  sources,
  resources,
  include_directories: include_directories('.'),
  dependencies: [gtk, gio_unix, exo, xfconf, xfceutil, xfceui, xfcepanel, xfcewindowing, xfcewindowingui, x11, xext],
  c_args: ['-DHAVE_CONFIG_H', '-DG_LOG_DOMAIN="libminios-tasklist"'],
  link_args: ['-Wl,-z,defs'],
  install: true,
  install_dir: plugin_dir,
)

i18n.merge_file(
  input: 'minios-tasklist.desktop.in.in',
  output: 'minios-tasklist.desktop',
  po_dir: 'po',
  type: 'desktop',
  install: true,
  install_dir: desktop_dir,
)

subdir('po')
