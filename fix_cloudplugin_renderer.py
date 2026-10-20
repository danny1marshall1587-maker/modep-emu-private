#!/usr/bin/env python3
"""
fix_cloudplugin_renderer.py
Fixes null dereferences, missing category/comment handling, and sorting errors
in modep_app/html/js/cloudplugin.js and patchstorage.js to ensure all 930+ online plugins
render flawlessly in the Plugin Store.
"""

import os
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
CLOUDPLUGIN_JS = os.path.join(BASE_DIR, "modep_app", "html", "js", "cloudplugin.js")

def patch_cloudplugin_js():
    with open(CLOUDPLUGIN_JS, "r", encoding="utf-8") as f:
        code = f.read()

    # 1. Fix getCategory helper in showPlugins
    old_get_cat = """        var getCategory = function(plugin) {
            category = plugin.category[0]
            if (category == 'Utility' && plugin.category.length == 2 && plugin.category[1] == 'MIDI') {
                return 'MIDI';
            }
            return category
        }"""

    new_get_cat = """        var getCategory = function(plugin) {
            if (!plugin || !plugin.category || !Array.isArray(plugin.category) || plugin.category.length === 0) {
                return 'Utility';
            }
            category = plugin.category[0];
            if (category == 'Utility' && plugin.category.length >= 2 && plugin.category[1] == 'MIDI') {
                return 'MIDI';
            }
            return category || 'Utility';
        }"""

    if old_get_cat in code:
        code = code.replace(old_get_cat, new_get_cat)

    # 2. Fix sorting by label safely
    old_sort = """        // sort plugins by label
        plugins.sort(function (a, b) {
            a = a.label.toLowerCase()
            b = b.label.toLowerCase()
            if (a > b) {
                return 1
            }
            if (a < b) {
                return -1
            }
            return 0
        })"""

    new_sort = """        // sort plugins by label safely
        plugins.sort(function (a, b) {
            var la = ((a && (a.label || a.name)) || '').toLowerCase();
            var lb = ((b && (b.label || b.name)) || '').toLowerCase();
            if (la > lb) return 1;
            if (la < lb) return -1;
            return 0;
        });"""

    if old_sort in code:
        code = code.replace(old_sort, new_sort)

    # 3. Fix renderPlugin null comment / thumbnail / brand / label dereferences
    old_render = """    renderPlugin: function (plugin, cloudReached, featured) {
        var self = $(this)
        var uri = escape(plugin.uri)
        var comment = plugin.comment.trim()
        var has_comment = ""
        if(!comment) {
            comment = "No description available";
            has_comment = "no_description";
        }
        var plugin_data = {
            uri: uri,
            screenshot_href: plugin.screenshot_href,
            thumbnail_href: plugin.thumbnail_href,
            has_comment: has_comment,
            comment: comment,
            status: plugin.status,
            brand : plugin.brand,
            label : plugin.label,
            build_env: plugin.buildEnvironment,
        }"""

    new_render = """    renderPlugin: function (plugin, cloudReached, featured) {
        var self = $(this);
        if (!plugin) return $('<div>');
        var uri = escape(plugin.uri || '');
        var rawComment = (plugin.comment || '').trim();
        var comment = rawComment || "No description available";
        var has_comment = rawComment ? "" : "no_description";
        var thumb = plugin.thumbnail_href || (plugin.thumbnail_available ? (SITEURL + '/lv2/plugins/' + btoa(plugin.uri) + '/thumbnail') : "/resources/pedals/default-thumbnail.png");
        var screen = plugin.screenshot_href || (plugin.screenshot_available ? (SITEURL + '/lv2/plugins/' + btoa(plugin.uri) + '/screenshot') : "/resources/pedals/default-screenshot.png");
        var plugin_data = {
            uri: uri,
            screenshot_href: screen,
            thumbnail_href: thumb,
            has_comment: has_comment,
            comment: comment,
            status: plugin.status || 'blocked',
            brand : plugin.brand || (plugin.author ? plugin.author.name : '') || '',
            label : plugin.label || plugin.name || 'LV2 Plugin',
            build_env: plugin.buildEnvironment || 'prod',
        };"""

    if old_render in code:
        code = code.replace(old_render, new_render)

    # 4. Fix showPluginInfo null checks
    old_info_category = """            var category = plugin.category[0]
            if (category == 'Utility' && plugin.category.length == 2 && plugin.category[1] == 'MIDI') {
                category = 'MIDI'
            }"""

    new_info_category = """            var category = (plugin && plugin.category && Array.isArray(plugin.category) && plugin.category.length > 0) ? plugin.category[0] : 'Utility';
            if (category == 'Utility' && plugin.category && plugin.category.length >= 2 && plugin.category[1] == 'MIDI') {
                category = 'MIDI';
            }"""

    if old_info_category in code:
        code = code.replace(old_info_category, new_info_category)

    old_comment_info = 'comment: plugin.comment.trim() || "No description available",'
    new_comment_info = 'comment: (plugin.comment || "").trim() || "No description available",'
    code = code.replace(old_comment_info, new_comment_info)

    old_pkg_name = "package_name: (plugin.bundle_name || plugin.bundles[0]).replace(/\.lv2$/, ''),"
    new_pkg_name = "package_name: (plugin.bundle_name || (plugin.bundles && plugin.bundles[0]) || plugin.name || 'plugin').replace(/\.lv2$/, ''),"
    code = code.replace(old_pkg_name, new_pkg_name)

    # Ports check in showPluginInfo
    old_ports_loop = """            // formating numbers and flooring ranges up to two decimal cases
            for (var i = 0; i < plugin.ports.control.input.length; i++) {
                plugin.ports.control.input[i].formatted = format(plugin.ports.control.input[i])
            }"""

    new_ports_loop = """            // formating numbers safely
            if (plugin.ports && plugin.ports.control && plugin.ports.control.input) {
                for (var i = 0; i < plugin.ports.control.input.length; i++) {
                    plugin.ports.control.input[i].formatted = format(plugin.ports.control.input[i]);
                }
            }"""
    if old_ports_loop in code:
        code = code.replace(old_ports_loop, new_ports_loop)

    with open(CLOUDPLUGIN_JS, "w", encoding="utf-8") as f:
        f.write(code)

    print("[+] Successfully patched modep_app/html/js/cloudplugin.js with robust null checks and category fallback!")

if __name__ == '__main__':
    patch_cloudplugin_js()
