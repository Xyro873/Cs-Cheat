// pch.h: Dies ist eine vorkompilierte Headerdatei.
// Die unten aufgeführten Dateien werden nur einmal kompiliert, um die Buildleistung für zukünftige Builds zu verbessern.
// Dies wirkt sich auch auf die IntelliSense-Leistung aus, Codevervollständigung und viele Features zum Durchsuchen von Code eingeschlossen.
// Die hier aufgeführten Dateien werden jedoch ALLE neu kompiliert, wenn mindestens eine davon zwischen den Builds aktualisiert wird.
// Fügen Sie hier keine Dateien hinzu, die häufig aktualisiert werden sollen, da sich so der Leistungsvorteil ins Gegenteil verkehrt.

#ifndef PCH_H
#define PCH_H

// Add precompiled headers
#include "framework.h"
#include <d3d11.h>
#include <dxgi.h>
#include "imgui-master/imgui-master/imgui.h"
#include "imgui-master/imgui-master/backends/imgui_impl_win32.h"
#include "imgui-master/imgui-master/backends/imgui_impl_dx11.h"
#include "minhook-master/minhook-master/include/MinHook.h"
#include "sdk/offsets.h"
#include "sdk/structs.h"

#endif //PCH_H
