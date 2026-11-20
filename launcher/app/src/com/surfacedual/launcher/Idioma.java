package com.surfacedual.launcher;

/** Textos en dos idiomas, juntos donde se usan: español si Android está en español; si no, inglés.
 *  El idioma de Android lo pone el agente con el elegido al instalar (o el de Windows). */
final class Idioma {
    static final String AL_NACER = java.util.Locale.getDefault().getLanguage();   // Inicio rehace el proceso si cambia (ventanas flotantes ya pintadas)
    static String t(String es, String en) { return "es".equals(java.util.Locale.getDefault().getLanguage()) ? es : en; }
}
