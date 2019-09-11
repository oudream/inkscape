// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_PREFERENCES_SKELETON_H
#define SEEN_PREFERENCES_SKELETON_H

#include "inkscape-version.h"

// FIXME why is this here?
#ifdef N_
#undef N_
#endif
#define N_(x) x

/* The root's "version" attribute describes the preferences file format version.
 * It should only increase when a backwards-incompatible change is made,
 * and special handling has to be added to the preferences class to update
 * obsolete versions the user might have. */
static char const preferences_skeleton[] =
R"=====(
<inkscape version="1"
  xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
  xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape">
  <group id="window">
    <group id="task" />
    <group id="menu"        state="1"/>
    <group id="commands"    state="1"/>
    <group id="snaptoolbox" state="1"/>
    <group id="toppanel"    state="1"/>
    <group id="toolbox"     state="1"/>
    <group id="statusbar"   state="1"/>
    <group id="panels"      state="1"/>
    <group id="rulers"      state="1"/>
    <group id="scrollbars"  state="1"/>
  </group>
  <group id="fullscreen">
    <group id="task" />
    <group id="menu"        state="1"/>
    <group id="commands"    state="1"/>
    <group id="snaptoolbox" state="1"/>
    <group id="toppanel"    state="1"/>
    <group id="toolbox"     state="1"/>
    <group id="statusbar"   state="1"/>
    <group id="panels"      state="1"/>
    <group id="rulers"      state="1"/>
    <group id="scrollbars"  state="1"/>
  </group>
  <group id="focus">
    <group id="task" />
    <group id="menu"        state="0"/>
    <group id="commands"    state="0"/>
    <group id="snaptoolbox" state="0"/>
    <group id="toppanel"    state="0"/>
    <group id="toolbox"     state="0"/>
    <group id="statusbar"   state="0"/>
    <group id="panels"      state="0"/>
    <group id="rulers"      state="0"/>
    <group id="scrollbars"  state="0"/>
  </group>

  <group id="template">
    <sodipodi:namedview
       id="base"
       pagecolor="#ffffff"
       bordercolor="#666666"
       borderopacity="1.0"
       objecttolerance="10.0"
       gridtolerance="10.0"
       guidetolerance="10.0"
       inkscape:pageopacity="0.0"
       inkscape:pageshadow="2"
       inkscape:window-width="640"
       inkscape:window-height="480" />
  </group>

  <group id="tools" bounding_box="0">

    <group id="shapes" style="fill-rule:evenodd;" selcue="1" gradientdrag="1">
      <eventcontext id="rect" style="fill:blue;" usecurrent="1"/>
      <eventcontext id="3dbox" style="stroke:none;stroke-linejoin:round;" usecurrent="1">
        <side id="XYfront"  style="fill:#8686bf;stroke:none;stroke-linejoin:round;" usecurrent="0"/>
        <side id="XYrear"   style="fill:#e9e9ff;stroke:none;stroke-linejoin:round;" usecurrent="0"/>
        <side id="XZtop"    style="fill:#4d4d9f;stroke:none;stroke-linejoin:round;" usecurrent="0"/>
        <side id="XZbottom" style="fill:#afafde;stroke:none;stroke-linejoin:round;" usecurrent="0"/>
        <side id="YZright"  style="fill:#353564;stroke:none;stroke-linejoin:round;" usecurrent="0"/>
        <side id="YZleft"   style="fill:#d7d7ff;stroke:none;stroke-linejoin:round;" usecurrent="0"/>
      </eventcontext>
      <eventcontext id="arc" style="fill:red;" end="0" start="0" usecurrent="1"/>
      <eventcontext id="star" magnitude="5" style="fill:yellow;" usecurrent="1"/>
      <eventcontext id="spiral" style="fill:none;stroke:black" expansion="1" usecurrent="0"/>
    </group>

    <group id="freehand"
         style="fill:none;stroke:black;stroke-opacity:1;stroke-linejoin:miter;stroke-linecap:butt;">
      <eventcontext id="pencil" tolerance="4.0" selcue="1" style="stroke-width:1px;" usecurrent="0" average_all_sketches="1"/>
      <eventcontext id="pen" mode="drag" selcue="1" style="stroke-width:1px;" usecurrent="0"/>
    </group>

    <eventcontext id="calligraphic" style="fill:black;fill-opacity:1;fill-rule:nonzero;stroke:none;"
                       mass="2" angle="30" width="15" thinning="10" flatness="90" cap_rounding="0.0" usecurrent="1"
                       tracebackground="0" usepressure="1" usetilt="0" keep_selected="1">

      <group id="preset">
        <group id="cp0" name="Dip pen" mass="2" wiggle="0.0" angle="30.0" thinning="10" tremor="0.0" flatness="90" cap_rounding="0.0" tracebackground="0" usepressure="1" usetilt="1" />
        <group id="cp1" name="Marker" mass="2" wiggle="0.0" angle="90.0" thinning="0.0" tremor="0.0" flatness="0.0" cap_rounding="1.0" tracebackground="0" usepressure="0" usetilt="0" />
        <group id="cp2" name="Brush" mass="2" wiggle="25" angle="45.0" thinning="-40" tremor="0.0" flatness="16" cap_rounding=".1" tracebackground="0" usepressure="1" usetilt="1" />
        <group id="cp3" name="Wiggly" usetilt="1" tracebackground="0" usepressure="1" cap_rounding="0.1" flatness="16" tremor="18" thinning="-30" angle="30" wiggle="50" mass="0" />
        <group id="cp4" name="Splotch" width="100" usetilt="1" tracebackground="0" usepressure="0" cap_rounding="1" flatness="0" tremor="10" thinning="30" angle="30" wiggle="0" mass="0" />
        <group id="cp5" name="Tracing" width="50" mass="0" wiggle="0.0" angle="0.0" thinning="0.0" tremor="0.0" flatness="0" cap_rounding="0.0" tracebackground="1" usepressure="1" usetilt="1"/>
      </group>
    </eventcontext>

    <eventcontext id="eraser" mode="1" style="fill:#ff0000;fill-opacity:1;fill-rule:nonzero;stroke:none;"
                       mass="0.02" drag="1" angle="30" width="10" thinning="0.1" flatness="0.0" cap_rounding="1.4" usecurrent="0"
                       tracebackground="0" usepressure="1" usetilt="0" selcue="1">
    </eventcontext>

    <eventcontext id="lpetool" mode="drag" style="fill:#ff0000;fill-opacity:1;fill-rule:nonzero;stroke:none;">
    </eventcontext>

    <eventcontext id="text"  usecurrent="0" gradientdrag="1"
                       font_sample="AaBbCcIiPpQq12369$\342\202\254\302\242?.;/()"
                       show_sample_in_list="1" use_svg2="0"
                  style="fill:black;fill-opacity:1;line-height:1.25;stroke:none;font-family:sans-serif;font-style:normal;font-weight:normal;font-size:40px;" selcue="1"/>

    <eventcontext id="nodes" selcue="1" gradientdrag="1" highlight_color="0xff0000ff" pathflash_enabled="1" pathflash_unselected="0" pathflash_timeout="500" show_handles="1" show_outline="0"
      sculpting_profile="1" single_node_transform_handles="0" show_transform_handles="0" live_outline="1" live_objects="1" show_helperpath="0" x="0" y="0" edit_clipping_paths="0" edit_masks="0" />
    <eventcontext id="tweak" selcue="0" gradientdrag="0" show_handles="0" width="0.2" force="0.2" fidelity="0.5" usepressure="1" style="fill:red;stroke:none;" usecurrent="0"/>
    <eventcontext id="spray" selcue="1" gradientdrag="0" usepressure="1" width="15" population="70" mode="1" rotation_variation="0" scale_variation="0" standard_deviation="70" mean="0"/>
    <eventcontext id="gradient" selcue="1"/>
    <eventcontext id="mesh" selcue="1"/>
    <eventcontext id="zoom" selcue="1" gradientdrag="0"/>
    <eventcontext id="dropper" selcue="1" gradientdrag="1" pick="1" setalpha="1"/>
    <eventcontext id="select" selcue="1" gradientdrag="0"/>
    <eventcontext id="connector" style="fill:none;fill-rule:evenodd;stroke:black;stroke-opacity:1;stroke-linejoin:miter;stroke-width:1px;stroke-linecap:butt;" selcue="1"/>
    <eventcontext id="paintbucket" style="fill:#a0a0a0;stroke:none;" usecurrent="1"/>
  </group>

  <group id="palette">
    <group id="dashes">
      <dash id="solid" style="stroke-dasharray:none"/>
      <dash id="dash-1-1" style="stroke-dasharray:1,1"/>
      <dash id="dash-1-2" style="stroke-dasharray:1,2"/>
      <dash id="dash-1-3" style="stroke-dasharray:1,3"/>
      <dash id="dash-1-4" style="stroke-dasharray:1,4"/>
      <dash id="dash-1-6" style="stroke-dasharray:1,6"/>
      <dash id="dash-1-8" style="stroke-dasharray:1,8"/>
      <dash id="dash-1-12" style="stroke-dasharray:1,12"/>
      <dash id="dash-1-24" style="stroke-dasharray:1,24"/>
      <dash id="dash-1-48" style="stroke-dasharray:1,48"/>
      <dash id="dash-empty" style="stroke-dasharray:0 11"/>
      <dash id="dash-2-1" style="stroke-dasharray:2,1"/>
      <dash id="dash-3-1" style="stroke-dasharray:3,1"/>
      <dash id="dash-4-1" style="stroke-dasharray:4,1"/>
      <dash id="dash-6-1" style="stroke-dasharray:6,1"/>
      <dash id="dash-8-1" style="stroke-dasharray:8,1"/>
      <dash id="dash-12-1" style="stroke-dasharray:12,1"/>
      <dash id="dash-24-1" style="stroke-dasharray:24,1"/>
      <dash id="dash-2-2" style="stroke-dasharray:2,2"/>
      <dash id="dash-3-3" style="stroke-dasharray:3,3"/>
      <dash id="dash-4-4" style="stroke-dasharray:4,4"/>
      <dash id="dash-6-6" style="stroke-dasharray:6,6"/>
      <dash id="dash-8-8" style="stroke-dasharray:8,8"/>
      <dash id="dash-12-12" style="stroke-dasharray:12,12"/>
      <dash id="dash-24-24" style="stroke-dasharray:24,24"/>
      <dash id="dash-2-4" style="stroke-dasharray:2,4"/>
      <dash id="dash-4-2" style="stroke-dasharray:4,2"/>
      <dash id="dash-2-6" style="stroke-dasharray:2,6"/>
      <dash id="dash-6-2" style="stroke-dasharray:6,2"/>
      <dash id="dash-4-8" style="stroke-dasharray:4,8"/>
      <dash id="dash-8-4" style="stroke-dasharray:8,4"/>
      <dash id="dash-2-1-012-1" style="stroke-dasharray:2,1,0.5,1"/>
      <dash id="dash-4-2-1-2" style="stroke-dasharray:4,2,1,2"/>
      <dash id="dash-8-2-1-2" style="stroke-dasharray:8,2,1,2"/>
      <dash id="dash-012-012" style="stroke-dasharray:0.5,0.5"/>
      <dash id="dash-014-014" style="stroke-dasharray:0.25,0.25"/>
      <dash id="dash-0110-0110" style="stroke-dasharray:0.1,0.1"/>
    </group>
  </group>

  <group id="colorselector" page="0"/>

  <group id="embedded">
    <group id="swatches"
      panel_size="1"
      panel_mode="1"
      panel_ratio="100"
      panel_wrap="0"
      palette="Inkscape default" />
  </group>

  <group id="dialogs">
    <group id="toolbox"/>
    <group id="fillstroke"/>
    <group id="filtereffects"/>
    <group id="textandfont"/>
    <group id="transformation" applyseparately="0"/>
    <group id="align"/>
    <group id="xml"/>
    <group id="find"/>
    <group id="spellcheck" w="200" h="250" ignorenumbers="1"/>
    <group id="documentoptions" state="1"/>
    <group id="preferences" state="1"/>
    <group id="gradienteditor"/>
    <group id="object"/>
    <group id="export" default="" append_extension="1" path="">
      <group id="exportarea"/>
      <group id="defaultxdpi"/>
    </group>
    <group id="save_as" default="" append_extension="1" enable_preview="1" path="" use_current_dir="1"/>
    <group id="save_copy" default="" append_extension="1" enable_preview="1" path=""/>
    <group id="open" enable_preview="1" path=""/>
    <group id="import" enable_preview="1" path="" ask="1" ask_svg="1" link="link" scale="optimizeSpeed"/>
    <group id="debug" redirect="0"/>
    <group id="clonetiler" />
    <group id="gridtiler" />
    <group id="extension-error" show-on-startup="0"/>
    <group id="memory" />
    <group id="messages" />
    <group id="swatches" />
    <group id="iconpreview" />
    <group id="aboutextensions" />
    <group id="treeeditor" />
    <group id="layers" maxDepth="20"/>
    <group id="extensioneditor" />
    <group id="trace" state="1" />
    <group id="script" />
    <group id="input" />
    <group id="colorpickerwindow" />
    <group id="undo-history" />
    <group id="transparency"
       on-focus="1.0"
       on-blur="0.50"
       animate-time="100" />
  </group>
  <group id="printing">
    <settings id="ps"/>
    <group id="debug" add-label-comments="0"/>
  </group>

  <group id="options">
    <group id="renderingcache" size="64" />
    <group id="useoldpdfexporter" value="0" />
    <group id="highlightoriginal" value="1" />
    <group id="relinkclonesonduplicate" value="0" />
    <group id="mapalt" value="1" />
    <group id="trackalt" value="0" />
    <group id="switchonextinput" value="0" />
    <group id="useextinput" value="1" />
    <group id="nudgedistance" value="2px"/>
    <group id="rotationsnapsperpi" value="12"/>
    <group id="cursortolerance" value="8.0"/>
    <group id="dragtolerance" value="4.0"/>
    <group id="grabsize" value="3"/>
    <group
       id="displayprofile"
       enable="0"
       from_display="0"
       intent="0"
       uri="" />
    <group
       id="softproof"
       enable="0"
       intent="0"
       gamutcolor="#808080"
       gamutwarn="0"
       bpc="0"
       preserveblack="0"
       uri="" />
    <group id="savewindowgeometry" value="1"/>
    <group id="defaultoffsetwidth" value="2px"/>
    <group id="defaultscale" value="2px"/>
    <group id="maxrecentdocuments" value="36"/>
    <group id="zoomincrement" value="1.414213562"/>
    <group id="zoomcorrection" value="1.0" unit="mm"/>
    <group id="keyscroll" value="15"/>
    <group id="wheelscroll" value="40"/>
    <group id="spacebarpans" value="1"/>
    <group id="wheelzooms" value="0"/>
    <group id="transientpolicy" value="1"/>
    <group id="scrollingacceleration" value="0.4"/>
    <group id="snapdelay" value="150"/>
    <group id="snapweight" value="0.5"/>
    <group id="snapclosestonly" value="0"/>
    <group id="snapindicator" value="1"/>
    <group id="autoscrollspeed" value="0.7"/>
    <group id="autoscrolldistance" value="-10"/>
    <group id="simplifythreshold" value="0.002"/>
    <group id="bitmapeditor" value="gimp"/>
    <group id="svgeditor" value="inkscape"/>
    <group id="bitmapautoreload" value="1"/>
    <group id="dialogtype" value="1"/>
    <group id="dock"
           cancenterdock="1"
           dockbarstyle="2"
           switcherstyle="2"/>
    <group id="dialogsskiptaskbar" value="1"/>
    <group id="arenatilescachesize" value="8192"/>
    <group id="preservetransform" value="0"/>
    <group id="clonecompensation" value="1"/>
    <group id="cloneorphans" value="0"/>
    <group id="stickyzoom" value="0"/>
    <group id="selcue" value="2"/>
    <group id="transform" stroke="1" rectcorners="1" pattern="1" gradient="1" />
    <group id="dash" scale="1" />
    <group id="kbselection" inlayer="1" onlyvisible="1" onlysensitive="1" />
    <group id="selection" layerdeselect="1" />
    <group id="createbitmap"/>
    <group id="compassangledisplay" value="0"/>
    <group id="maskobject" topmost="1" remove="1"/>
    <group id="blurquality" value="0"/>
    <group id="filterquality" value="1"/>
    <group id="showfiltersinfobox" value="1" />
    <group id="startmode" outline="0"/>
    <group id="outlinemode" value="0"/>
    <group id="ocalurl" str="openclipart.org"/>
    <group id="ocalusername" str=""/>
    <group id="ocalpassword" str=""/>

    <group id="wireframecolors"
           onlight="0x000000ff"
           ondark="0xffffffff"
           images="0xff0000ff"
           clips="0x00ff00ff"
           masks="0x0000ffff"/>
    <group id="svgoutput"
           disable_optimizations="0"
           usenamedcolors="0"
           numericprecision="8"
           minimumexponent="-8"
           inlineattrs="0"
           indent="2"
           pathstring_format="2"
           forcerepeatcommands="0"
           incorrect_attributes_warn="1"
           incorrect_attributes_remove="0"
           incorrect_style_properties_warn="1"
           incorrect_style_properties_remove="0"
           style_defaults_warn="1"
           style_defaults_remove="0"
           check_on_reading="0"
           check_on_editing="0"
           check_on_writing="0"
           sort_attributes="0"/>
    <group id="externalresources">
      <group id="xml"
           allow_net_access="0"/>
    </group>
    <group id="forkgradientvectors" value="1"/>
    <group id="iconrender" named_nodelay="0"/>
    <group id="autosave" enable="1" interval="10" path="" max="10"/>
    <group id="grids"
      no_emphasize_when_zoomedout="0">
      <group id="xy"
             units="px"
             origin_x="0.0"
             origin_y="0.0"
             spacing_x="1.0"
             spacing_y="1.0"
             color="0x3F3FFF20"
             empcolor="0x3F3FFF40"
             empspacing="5"
             dotted="0"/>
      <group id="axonom"
             units="mm"
             origin_x="0.0"
             origin_y="0.0"
             spacing_y="1.0"
             angle_x="30.0"
             angle_z="30.0"
             color="0x3F3FFF20"
             empcolor="0x3F3FFF40"
             empspacing="5"/>
    </group>
    <group id="workarounds"
           colorsontop="0"
           partialdynamic="0"/>
  </group>

  <group id="extensions">
  </group>

  <group id="desktop"
         style="">
    <group
       width="640"
       height="480"
       x="0"
       y="0"
       fullscreen="0"
       id="geometry" />
    <group
       id="XYfront" />
    <group
       id="XYrear" />
    <group
       id="XZtop" />
    <group
       id="XZbottom" />
    <group
       id="YZleft" />
    <group
       id="YZright" />
  </group>

  <group id="devices">
  </group>

  <group
     id="toolbox"
     icononly="1"
     secondary="1"
     small="1">
    <group
       id="tools"
       icononly="1"
       small="0" />
  </group>

  <group
     id="iconpreview"
     autoRefresh="1"
     pack="1"
     selectionHold="1"
     showFrames="1"
     selectionOnly="0">
    <group
       id="sizes">
      <group
         id="default">
        <group
           value="16"
           show="1"
           id="size16" />
        <group
           value="22"
           show="0"
           id="size22" />
        <group
           value="24"
           show="1"
           id="size24" />
        <group
           value="32"
           show="1"
           id="size32" />
        <group
           value="48"
           show="1"
           id="size48" />
        <group
           value="50"
           show="0"
           id="size50" />
        <group
           value="64"
           show="0"
           id="size64" />
        <group
           value="72"
           show="0"
           id="size72" />
        <group
           value="80"
           show="0"
           id="size80" />
        <group
           value="96"
           show="0"
           id="size96" />
        <group
           value="128"
           show="1"
           id="size128" />
        <group
           value="256"
           show="0"
           id="size256" />
      </group>
    </group>
  </group>
  <group id="debug">
    <group id="latency" skew="1"/>
  </group>
  <group id="ui"
    language=""/>

</inkscape>
)=====";

#define PREFERENCES_SKELETON_SIZE (sizeof(preferences_skeleton) - 1)

// Raw string literal cannot contain translatable strings. Fortunately, we only translate
// calligraphy presets.
// Note: actual translation is done in CalligraphyToolbar::build_presets_list(), we just
// mark the strings as translatable here (see GitLab issue 128):
Glib::ustring calligraphy_name_array[] = {
    _("Dip pen"),
    _("Marker"),
    _("Brush"),
    _("Wiggly"),
    _("Splotchy"),
    _("Tracing")
};

#endif /* !SEEN_PREFERENCES_SKELETON_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :