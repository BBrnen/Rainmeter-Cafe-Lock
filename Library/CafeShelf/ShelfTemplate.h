#pragma once
// ShelfSuite v2.1, d4f186ba0b5c262c7559b80841132f5fd3884f3c, Shelf1/Shelf.ini.
// Copyright (c) 2025 Martin Santos, MIT. See UI/LICENSE-ShelfSuite.txt.
// Only static descriptive metadata is adapted. No installed template is trusted.
namespace CafeShelf {
static const char ShelfTemplate[] = R"CAFE_TEMPLATE([Rainmeter]
Update=1000
AccurateText=1
DynamicWindowSize=1
BackgroundMode=2
SolidColor=0,0,0,1
Draggable=1
OnRefreshAction=[!CommandMeasure MeasureEngine "SwitchTab(1)"]
ContextTitle=Theme: DeepOcean
ContextAction=[!WriteKeyValue "Rainmeter" "@IncludeTheme" "#@#Themes\DeepOcean.inc"][!Refresh]
ContextTitle2=Theme: Forest
ContextAction2=[!WriteKeyValue "Rainmeter" "@IncludeTheme" "#@#Themes\Forest.inc"][!Refresh]
ContextTitle3=Theme: Terracotta
ContextAction3=[!WriteKeyValue "Rainmeter" "@IncludeTheme" "#@#Themes\Terracotta.inc"][!Refresh]
ContextTitle4=Theme: Obsidian
ContextAction4=[!WriteKeyValue "Rainmeter" "@IncludeTheme" "#@#Themes\Obsidian.inc"][!Refresh]
ContextTitle5=---
ContextTitle6=Open Configurator
ContextAction6=["#@#configurator.html"]
@IncludeVars=#@#Variables.inc
@IncludeTheme=#@#Themes\DeepOcean.inc

[Metadata]
Name=ShelfSuite — New shelf
Author=Martin Santos
Information=Modular desktop organizer — Custom shelf
License=MIT
Version=2.0

[Variables]
; Override height (auto-calculated by Lua)
WidgetHeight=170

; ========================================
; LUA ENGINE
; ========================================
[MeasureEngine]
Measure=Script
ScriptFile=#@#ShelfEngine.lua
; Config loading: engine tries config.lua first, falls back to config.example.lua

; ========================================
; BACKGROUND
; ========================================
[MeterBackground]
Meter=Shape
Shape=Rectangle 0,0,#WidgetWidth#,#WidgetHeight#,#CornerRadius# | Fill Color #MainBackground# | StrokeWidth 0

; ========================================
; SETTINGS GEAR — opens configurator
; ========================================
[MeterSettingsGear]
Meter=String
Text=[\x2699]
X=(#WidgetWidth#-28)
Y=12
FontColor=255,255,255,80
FontFace=Segoe UI Symbol
FontSize=13
StringAlign=CENTER
AntiAlias=1
MouseOverAction=[!SetOption MeterSettingsGear FontColor "255,255,255,200"][!UpdateMeter MeterSettingsGear][!Redraw]
MouseLeaveAction=[!SetOption MeterSettingsGear FontColor "255,255,255,80"][!UpdateMeter MeterSettingsGear][!Redraw]
LeftMouseUpAction=["#@#configurator.html"]
ToolTipText=Open ShelfSuite Configurator

; ========================================
; TABS (up to 5 — names and visibility managed by Lua)
; ========================================
[StyleTabBg]
Y=#PaddingY#
DynamicVariables=1
Hidden=1

[StyleTabText]
StringAlign=CENTERCENTER
FontFace=#FontName#
FontSize=#FontSize_Tab#
AntiAlias=1
DynamicVariables=1
Hidden=1

[MeterTab1Bg]
Meter=Shape
MeterStyle=StyleTabBg
X=#PaddingX#
Shape=Rectangle 0,0,#TabWidth#,#TabHeight#,#CornerRadius#,#CornerRadius# | Fill Color #ActiveTabColor# | StrokeWidth 0
LeftMouseUpAction=[!CommandMeasure MeasureEngine "SwitchTab(1)"]

[MeterTab1Text]
Meter=String
MeterStyle=StyleTabText
Text=Tab1
X=(#PaddingX#+(#TabWidth#/2))
Y=(#PaddingY#+(#TabHeight#/2))
FontColor=#ActiveTextColor#

[MeterTab2Bg]
Meter=Shape
MeterStyle=StyleTabBg
X=(#PaddingX#+#TabWidth#+#TabSpacing#)
Shape=Rectangle 0,0,#TabWidth#,#TabHeight#,#CornerRadius#,#CornerRadius# | Fill Color #InactiveTabColor# | StrokeWidth 0
LeftMouseUpAction=[!CommandMeasure MeasureEngine "SwitchTab(2)"]

[MeterTab2Text]
Meter=String
MeterStyle=StyleTabText
Text=Tab2
X=(#PaddingX#+#TabWidth#+#TabSpacing#+(#TabWidth#/2))
Y=(#PaddingY#+(#TabHeight#/2))
FontColor=#InactiveTextColor#

[MeterTab3Bg]
Meter=Shape
MeterStyle=StyleTabBg
X=(#PaddingX#+(#TabWidth#+#TabSpacing#)*2)
Shape=Rectangle 0,0,#TabWidth#,#TabHeight#,#CornerRadius#,#CornerRadius# | Fill Color #InactiveTabColor# | StrokeWidth 0
LeftMouseUpAction=[!CommandMeasure MeasureEngine "SwitchTab(3)"]

[MeterTab3Text]
Meter=String
MeterStyle=StyleTabText
Text=Tab3
X=(#PaddingX#+(#TabWidth#+#TabSpacing#)*2+(#TabWidth#/2))
Y=(#PaddingY#+(#TabHeight#/2))
FontColor=#InactiveTextColor#

[MeterTab4Bg]
Meter=Shape
MeterStyle=StyleTabBg
X=(#PaddingX#+(#TabWidth#+#TabSpacing#)*3)
Shape=Rectangle 0,0,#TabWidth#,#TabHeight#,#CornerRadius#,#CornerRadius# | Fill Color #InactiveTabColor# | StrokeWidth 0
LeftMouseUpAction=[!CommandMeasure MeasureEngine "SwitchTab(4)"]

[MeterTab4Text]
Meter=String
MeterStyle=StyleTabText
Text=Tab4
X=(#PaddingX#+(#TabWidth#+#TabSpacing#)*3+(#TabWidth#/2))
Y=(#PaddingY#+(#TabHeight#/2))
FontColor=#InactiveTextColor#

[MeterTab5Bg]
Meter=Shape
MeterStyle=StyleTabBg
X=(#PaddingX#+(#TabWidth#+#TabSpacing#)*4)
Shape=Rectangle 0,0,#TabWidth#,#TabHeight#,#CornerRadius#,#CornerRadius# | Fill Color #InactiveTabColor# | StrokeWidth 0
LeftMouseUpAction=[!CommandMeasure MeasureEngine "SwitchTab(5)"]

[MeterTab5Text]
Meter=String
MeterStyle=StyleTabText
Text=Tab5
X=(#PaddingX#+(#TabWidth#+#TabSpacing#)*4+(#TabWidth#/2))
Y=(#PaddingY#+(#TabHeight#/2))
FontColor=#InactiveTextColor#

; ========================================
; ICON SLOTS (up to 48 — managed by Lua)
; ========================================
[StyleIcon]
W=#IconSize#
H=#IconSize#
Hidden=1

[StyleIconText]
FontColor=#IconTextColor#
FontFace=#FontName#
FontSize=#FontSize_Icon#
StringAlign=CENTERCENTER
AntiAlias=1
Hidden=1

[MeterIcon1]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon1Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon2]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon2Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon3]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon3Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon4]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon4Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon5]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon5Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon6]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon6Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon7]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon7Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon8]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon8Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon9]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon9Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon10]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon10Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon11]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon11Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon12]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon12Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon13]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon13Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon14]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon14Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon15]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon15Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon16]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon16Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon17]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon17Text]
Meter=String
MeterStyle=StyleIconText

[MeterIcon18]
Meter=Image
MeterStyle=StyleIcon
[MeterIcon18Text]
Meter=String
MeterStyle=StyleIconText
)CAFE_TEMPLATE";
}
