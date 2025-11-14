# Zgrzyt Wiki - Frontend

To jest interfejs użytkownika (frontend) dla projektu **Zgrzyt Wiki**. Aplikacja została zbudowana w celu archiwizacji i łatwego przeszukiwania segmentów z podcastu "Zgrzyt", prowadzonego przez Gimpera i Revo.

Głównym celem projektu była nauka i demonstracja umiejętności budowania aplikacji Full-Stack, w tym integracji z niestandardowym backendem, API YouTube oraz implementacji interaktywnego interfejsu użytkownika.

**➡️ Zobacz wersję live:** [https://zgrzytwiki.dev](https://zgrzytwiki.dev)

![Podgląd aplikacji Zgrzyt Wiki](image_066ac1.jpg)

---

## 🛠️ Użyte Technologie

Ten frontend został zbudowany przy użyciu:

* **Czysty JavaScript (ES6+):** Cała logika aplikacji, w tym pobieranie danych, renderowanie, filtrowanie i interakcje z API, została napisana w natywnym JavaScripcie, bez użycia zewnętrznych frameworków.
* **HTML5 i CSS3:** Semantyczna struktura i nowoczesne style (Flexbox, Grid) do stworzenia responsywnego interfejsu.
* **YouTube Iframe API:** Zintegrowane do dynamicznego tworzenia odtwarzaczy i odtwarzania wideo od konkretnego znacznika czasu.
* **Google Analytics (GA4):** Zaimplementowane do śledzenia ruchu na stronie.
* **GitHub Pages:** Aplikacja jest hostowana jako statyczna strona bezpośrednio z repozytorium GitHub.

---

## 🚀 Funkcjonalności

* **Globalne Wyszukiwanie:** Pasek wyszukiwania filtruje *wszystkie* filmy (tytuły, opisy, tagi) po stronie klienta, co zapewnia natychmiastowe wyniki.
* **Leniwe Ładowanie (Lazy Rendering):** Aby przyspieszyć start, strona początkowo ładuje tylko pierwszą partię filmów. Kolejne są dynamicznie doładowywane podczas scrollowania (`IntersectionObserver`).
* **Dynamiczny Odtwarzacz:** Kliknięcie na dowolny segment (np. `► 02:40`) natychmiast uruchamia wideo dokładnie w tym momencie.
* **System Sugestii (Community Fix):** Użytkownicy mogą zgłaszać poprawki do opisów segmentów za pomocą przycisku "Popraw". Sugestie są wysyłane do backendu (`POST /api/fix`) i czekają na moderację.

---

## ⚙️ Architektura Systemu

Ten frontend jest częścią większej architektury Full-Stack:

1.  **Frontend (Ten projekt):** Hostowany na GitHub Pages, komunikuje się z backendem.
2.  **Backend (API):** Aplikacja **Spring Boot (Java)** hostowana na **Google Cloud Run**, wystawiająca REST API.
3.  **Baza Danych:** **PostgreSQL** (hostowany na Google Cloud SQL), przechowująca filmy i segmenty.
4.  **ETL (Ingest):** Skrypt w **Pythonie**, który używa `yt-dlp` do pobierania danych i **Google Gemini API** do analizy transkrypcji i tagowania anegdot.

[Link do Repozytorium Backendu](https://github.com/mendelson8/ZgrzytBackend)

* **Kontakt:** `kuba.mendel@wp.pl`
* **LinkedIn:** (Wstaw tutaj link do swojego LinkedIn)
