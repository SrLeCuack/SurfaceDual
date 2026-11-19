#pragma once
// El writer solo necesita un sumidero para mensajes de diagnóstico.
namespace sdlog { struct Nulo { template<class T> Nulo& operator<<(const T&) { return *this; } }; }
#define LOG(nivel) ::sdlog::Nulo()
#define CHECK(condicion) if (condicion) {} else ::sdlog::Nulo()
