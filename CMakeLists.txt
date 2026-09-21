cmake_minimum_required(VERSION 3.21)

project(omarchy-calendar VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core DBus Gui Network NetworkAuth Qml Quick QuickControls2 Sql)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSECRET REQUIRED IMPORTED_TARGET libsecret-1)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(omarchy-calendar
    app/eventstore.cpp
    app/eventstore.h
    app/main.cpp
    app/preferences.cpp
    app/preferences.h
    app/themeprovider.cpp
    app/themeprovider.h
)

qt_add_resources(omarchy-calendar qml
    PREFIX "/"
    BASE app
    FILES
        app/qml/App.qml
        app/qml/components/CalendarButton.qml
        app/qml/components/EventCard.qml
        app/qml/components/ThemedTextField.qml
        app/qml/components/ThemedTextArea.qml
        app/qml/components/ThemedComboBox.qml
        app/qml/views/WeekView.qml
        app/qml/views/DayView.qml
        app/qml/views/MonthView.qml
        app/qml/views/AgendaView.qml
        app/qml/views/SearchView.qml
        app/qml/views/SettingsView.qml
)

target_link_libraries(omarchy-calendar PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

qt_add_executable(omarchy-calendar-service
    service/calendarservice.cpp
    service/calendarservice.h
    service/database.cpp
    service/database.h
    service/googleauth.cpp
    service/googleauth.h
    service/googlemutations.cpp
    service/googlemutations.h
    service/googlesync.cpp
    service/googlesync.h
    service/main.cpp
    service/secretstore.cpp
    service/secretstore.h
)

target_link_libraries(omarchy-calendar-service PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Network
    Qt6::NetworkAuth
    Qt6::Sql
    PkgConfig::LIBSECRET
)

install(TARGETS omarchy-calendar omarchy-calendar-service RUNTIME DESTINATION bin)
