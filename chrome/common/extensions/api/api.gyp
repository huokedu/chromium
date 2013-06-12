# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'api',
      'type': 'static_library',
      'sources': [
        '<@(schema_files)',
      ],
      # TODO(jschuh): http://crbug.com/167187 size_t -> int
      'msvs_disabled_warnings': [ 4267 ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'cc_dir': 'chrome/common/extensions/api',
        'root_namespace': 'extensions::api',

        'conditions': [
          ['OS=="android"', {
            'schema_files': [  # Only these files are used on Android.
              'activity_log_private.json',
              'omnibox.json',
              'storage.json',
              'usb.idl',
            ]
          }],
          ['chromeos==1', {
            'schema_files': [  # Additional files for Chrome OS.
              'file_browser_handler_internal.json',
              'rtc_private.idl',
            ]
          }],
          ['OS!="android"', {
            'schema_files': [  # Files for all platforms except Android.
              'alarms.idl',
              'activity_log_private.json',
              'app_current_window_internal.idl',
              'app_runtime.idl',
              'app_window.idl',
              'audio.idl',
              'autotest_private.idl',
              'bluetooth.idl',
              'bookmark_manager_private.json',
              'bookmarks.json',
              'chromeos_info_private.json',
              'cloud_print_private.json',
              'command_line_private.json',
              'content_settings.json',
              'context_menus.json',
              'cookies.json',
              'debugger.json',
              'developer_private.idl',
              'dial.idl',
              'downloads.idl',
              'echo_private.json',
              'downloads_internal.idl',
              'enterprise_platform_keys_private.json',
              'events.json',
              'experimental_accessibility.json',
              'experimental_discovery.idl',
              'experimental_dns.idl',
              'experimental_history.json',
              'experimental_identity.idl',
              'experimental_idltest.idl',
              'experimental_infobars.json',
              'location.idl',
              'experimental_media_galleries.idl',
              'experimental_record.json',
              'system_info_memory.idl',
              'experimental_system_info_storage.idl',
              'extension.json',
              'file_system.idl',
              'font_settings.json',
              'history.json',
              'i18n.json',
              'identity.idl',
              'identity_private.idl',
              'idle.json',
              'management.json',
              'manifest_types.json',
              'media_galleries.idl',
              'media_galleries_private.idl',
              'media_player_private.json',
              'metrics_private.json',
              'music_manager_private.idl',
              'networking_private.json',
              'notifications.idl',
              'omnibox.json',
              'page_capture.json',
              'page_launcher.idl',
              'permissions.json',
              'power.idl',
              'push_messaging.idl',
              'runtime.json',
              'serial.idl',
              'session_restore.json',
              'socket.idl',
              'storage.json',
              'sync_file_system.idl',
              'system_indicator.idl',
              'system_info_cpu.idl',
              'system_info_display.idl',
              'system_private.json',
              'tab_capture.idl',
              'tabs.json',
              'terminal_private.json',
              'test.json',
              'top_sites.json',
              'usb.idl',
              'wallpaper_private.json',
              'web_navigation.json',
              'web_request.json',
              'web_socket_proxy_private.json',
              'webview.json',
              'windows.json'
            ]
          }]
        ]
      },
      'dependencies': [
        '<(DEPTH)/skia/skia.gyp:skia',
        '<(DEPTH)/sync/sync.gyp:sync',
      ],
      'conditions': [
        ['OS=="android"', {
          'schema_files!': [
            'usb.idl',
          ],
        }],
        ['OS!="chromeos"', {
          'schema_files!': [
            'file_browser_handler_internal.json',
            'rtc_private.idl',
          ],
        }],
      ],
    },
  ],
}
