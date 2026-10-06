# AzerothCore: Módulo Character Scale

Este módulo para **AzerothCore (WotLK 3.3.5a)** permite a los jugadores ajustar dinámicamente el tamaño/escala de su personaje mediante un comando de chat, con persistencia en la base de datos.

## Características

* Comando `.character_scale <valor>` para cambiar el tamaño del personaje.
* Persistencia automática en la base de datos `characters` al conectar, cambiar de mapa o teletransportarse.
* Restricción de rango de escala configurable desde `mod_character_scale.conf`.
* Desactivación automática durante transformaciones (como formas de Druida) para evitar conflictos con otros módulos visuales.

## Instalación

1. Descarga o clona este repositorio dentro de la carpeta `modules/` de tu código fuente de AzerothCore:

    cd modules/
    git clone https://github.com/Leaf-Spring/mod-character-scale.git

2. Recompila el proyecto con CMake.

3. La tabla `character_scale` se creará automáticamente en la base de datos `characters` al iniciar `worldserver`.

4. (Opcional) Copia `conf/mod_character_scale.conf.dist` a la carpeta `configs/` de tu servidor como `mod_character_scale.conf` para personalizar los límites.

## Uso

En el juego, ejecuta:
* `.character_scale 1.2` (Establece el tamaño al 120%)
* `.character_scale 0.8` (Establece el tamaño al 80%)

---

## Apoyo
Si este módulo te ha sido de utilidad y deseas apoyar mi trabajo, puedes invitarme un café:
☕ [buymeacoffee.com/foliaprintempo](https://buymeacoffee.com/foliaprintempo)
