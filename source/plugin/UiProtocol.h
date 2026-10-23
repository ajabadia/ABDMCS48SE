// UiProtocol.h - Version del contrato JSON entre C++ y la WebView.
//
// El documento de estado y la lista de presets llevan este numero. La pagina
// comprueba que coincide antes de pintar nada: si el backend y la pagina se
// despliegan por separado, un cambio incompatible se detecta en el acto en
// lugar de mostrar controles que no hacen lo que aparentan.
//
// Regla: subir la version solo cuando el esquema cambia de forma incompatible
// (campos renombrados o eliminados, significado distinto). Anadir campos
// opcionales no obliga a subirla; la pagina debe ignorar los que no conozca.

#pragma once

namespace bdmcs48::plugin {

inline constexpr int kUiProtocolVersion = 1;

} // namespace bdmcs48::plugin
