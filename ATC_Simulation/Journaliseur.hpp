#pragma once
#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

using namespace std;

// journaliseur singleton thread safe
class Journaliseur {
public:
    // instance unique
    static Journaliseur& obtenirInstance() {
        static Journaliseur instance;
   return instance;
    }

    // pas de copie
    Journaliseur(const Journaliseur&) = delete;
    Journaliseur& operator=(const Journaliseur&) = delete;

    // journaliser un message
    void journaliser(const string& composant, const string& message, const string& niveau = "INFO") {
        lock_guard<mutex> verrou(mutex_);
        
        string horodatage = obtenirHorodatageActuel();
        string entreeJournal = formaterEntreeJournal(horodatage, niveau, composant, message);
    
    // affichage console
        cout << entreeJournal << endl;
  
   // ecriture fichier json
        ecrireDansFichier(horodatage, niveau, composant, message);
    }

    // fermeture propre
    void fermer() {
        lock_guard<mutex> verrou(mutex_);
        if (fichierJournal_.is_open()) {
       fichierJournal_.close();
     }
    }

private:
    mutex mutex_;
    ofstream fichierJournal_;

    // constructeur prive
    Journaliseur() {
   fichierJournal_.open("journaux.json", ios::out | ios::app);
        if (!fichierJournal_.is_open()) {
         cerr << "erreur ouverture journaux.json" << endl;
        }
    }

    ~Journaliseur() {
     if (fichierJournal_.is_open()) {
        fichierJournal_.close();
        }
    }

    // horodatage actuel
    string obtenirHorodatageActuel() {
        auto maintenant = chrono::system_clock::now();
  auto temps = chrono::system_clock::to_time_t(maintenant);
        auto ms = chrono::duration_cast<chrono::milliseconds>(
            maintenant.time_since_epoch()) % 1000;
        
        stringstream ss;
        ss << put_time(localtime(&temps), "%Y-%m-%d %H:%M:%S");
        ss << '.' << setfill('0') << setw(3) << ms.count();
        return ss.str();
    }

    // format journal console
    string formaterEntreeJournal(const string& horodatage, const string& niveau,
        const string& composant, const string& message) {
   return "[" + horodatage + "] [" + niveau + "] [" + composant + "] " + message;
    }

    // ecriture json
    void ecrireDansFichier(const string& horodatage, const string& niveau,
        const string& composant, const string& message) {
   if (fichierJournal_.is_open()) {
     fichierJournal_ << "{";
        fichierJournal_ << "\"horodatage\":\"" << horodatage << "\",";
            fichierJournal_ << "\"niveau\":\"" << niveau << "\",";
            fichierJournal_ << "\"composant\":\"" << composant << "\",";
    fichierJournal_ << "\"message\":\"" << echapperJson(message) << "\"";
 fichierJournal_ << "}" << endl;
            fichierJournal_.flush();
        }
    }

    // echapper caracteres speciaux json
    string echapperJson(const string& str) {
      string resultat;
      for (char c : str) {
 switch (c) {
    case '"':  resultat += "\\\""; break;
     case '\\': resultat += "\\\\"; break;
 case '\b': resultat += "\\b";  break;
   case '\f': resultat += "\\f";  break;
      case '\n': resultat += "\\n";  break;
        case '\r': resultat += "\\r";  break;
         case '\t': resultat += "\\t";  break;
             default:   resultat += c;      break;
            }
        }
        return resultat;
    }
};
