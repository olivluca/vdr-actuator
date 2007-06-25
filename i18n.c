/*
 * i18n.c: Internationalization
 *
 * See the README file for copyright information and how to reach the author.
 * Russian: Vyacheslav Dikonov <sdiconov@mail.ru>
 *
 * $Id: 
 */

#include "i18n.h"

const tI18nPhrase Phrases[] = {
  { "Drive East",                                //English
    "Nach Osten",                                //German
    "",                                          //Slovenian
    "Muove a Est",                               //Italian
    "Draai naar Oost",                           //Dutch
    "",                                          //Portuguese
    "Aller Est",                                 //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Mueve a Este",                              //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian  
    "",                                          //Hugarian
    "Mou cap a l'est",                           //Catalan
    "+ï ð÷ÀÁ÷ç",                                 //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk     
  },
  { "Halt",                                      //English
    "Anhalten",                                  //German
    "",                                          //Slovenian
    "Ferma",                                     //Italian
    "Halt",                                      //Dutch
    "",                                          //Portuguese
    "Arrêt",                                     //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Para",                                      //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Atura",                                     //Catalan
    "àÁ÷ú",                                      //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Drive West",                                //English
    "Nach Westen",                               //German
    "",                                          //Slovenian
    "Muove a Ovest",                             //Italian
    "Draai naar West",                           //Dutch
    "",                                          //Portuguese
    "Aller Ouest",                               //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Mueve a Oeste",                             //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Mou cap a l'oest",                          //Catalan
    "+ï âïúïò",                                  //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Recalc",                                    //English
    "Recalc",                                    //German
    "",                                          //Slovenian
    "Ricalcola",                                 //Italian
    "Hercalculeer",                              //Dutch
    "",                                          //Portugues
    "Recalc",                                    //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Recalcula",                                 //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Recalcula",                                 //Catalan
    "©óÞóÀÃüÁïÁË",                               //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Goto %d",                                   //English
    "Gehe zu %d",                                //German
    "",                                          //Slovenian
    "Va a %d",                                   //Italian
    "Ga naar %d",                                //Dutch
    "",                                          //Portugues
    "Aller à %d",                                //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Ir a %d",                                   //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Anar a %d",                                 //Catalan
    "©óÞóûÁü %d",                                //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Store",                                     //English
    "Speichere",                                 //German
    "",                                          //Slovenian
    "Memorizza",                                 //Italian
    "Opslaan",                                   //Dutch
    "",                                          //Portugues
    "Enregistrer",                               //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Memoriza",                                  //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Memoritza",                                 //Catalan
    "Tïú÷ýùüÁË",                                 //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "%d Steps East",                             //English
    "%d Schritte Osten",                         //German
    "",                                          //Slovenian
    "%d Passi Est",                              //Italian
    "%d Stappen Oost",                           //Dutch
    "",                                          //Portugues
    "%d Incrément Est",                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "%d Pasos Este",                             //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "%d Passos Est",                             //Catalan
    "%d Õïñ(÷ð) ùï ð÷ÀÁ÷ç",                      //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "%d Steps West",                             //English
    "%d Schritte Westen",                        //German
    "",                                          //Slovenian
    "%d Passi Ovest",                            //Italian
    "%d Stappen West",                           //Dutch
    "",                                          //Portugues
    "%d Incrément Ouest",                        //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "%d Pasos Oeste",                            //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "%d Passos Oest",                            //Catalan
    "Õïñ(÷ð) ùï âïúïò",                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Set East Limit",                            //English
    "Ost-Limit setzen",                          //German
    "",                                          //Slovenian
    "Imposta Limite Est",                        //Italian
    "Zet Limiet Oost",                           //Dutch
    "",                                          //Portugues
    "Fixer Limite Est",                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Ajusta Limite Este",                        //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Ajusta Límit Est",                          //Catalan
    "¦÷ÀÁ÷ÃùÊû úÞóòóø",                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Set Zero",                                  //English
    "",                                          //German
    "",                                          //Slovenian
    "Messa a zero",                              //Italian
    "Zet op Nul",                                //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Puesta a cero",                             //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Posada a zero",                             //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Set West Limit",                            //English
    "West-Limit setzen",                         //German
    "",                                          //Slovenian
    "Imposta Limite Ovest",                      //Italian
    "Zet Limiet West",                           //Dutch
    "",                                          //Portugues
    "Fixer Limite Ouest",                        //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Ajusta Limite Oeste",                       //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Ajusta Límit Oest",                         //Catalan
    "TïúïòùÊû úÞóòóø",                           //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Frequency:",                                //English
    "Frequenz:",                                 //German
    "",                                          //Slovenian
    "Frequenza:",                                //Italian
    "Frequentie:",                               //Dutch
    "",                                          //Portugues
    "Fréquence:",                                //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Frecuencia:",                               //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Freqüència:",                               //Catalan
    "ãïÀÁ÷Áï:",                                  //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Symbolrate:",                               //English
    "Symbolrate:",                               //German
    "",                                          //Slovenian
    "Symbolrate:",                               //Italian
    "Symbolrate:",                               //Dutch
    "",                                          //Portugues
    "SymbolRate:",                               //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "SymbolRate:",                               //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "SymbolRate:",                               //Catalan
    "àüýð. Àç÷Þ÷ÀÁË",                            //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Vpid:",                                     //English
    "Vpid:",                                     //German
    "",                                          //Slovenian
    "Vpid:",                                     //Italian
    "Vpid:",                                     //Dutch
    "",                                          //Portugues
    "Vpid:",                                     //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Vpid:",                                     //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Vpid:",                                     //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Apid:",                                     //English
    "Apid:",                                     //German
    "",                                          //Slovenian
    "Apid:",                                     //Italian
    "Apid:",                                     //Dutch
    "",                                          //Portugues
    "Apid:",                                     //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Apid:",                                     //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Apid:",                                     //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Scan Transponder",                          //English
    "Scan Transponder",                          //German
    "",                                          //Slovenian
    "Scansione Transponder",                     //Italian
    "Scan Transponder",                          //Dutch
    "",                                          //Portugues
    "Scanner Transpondeur",                      //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Escaneo Transponder",                       //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Escaneja Transponder",                      //Catalan
    "àüýð. Àç÷Þ÷ÀÁË",                            //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Scan Satellite",                            //English
    "",                                          //German
    "",                                          //Slovenian
    "Scansione Satellite",                       //Italian
    "Scan satelliet",                            //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Escaneo Satélite",                          //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Escaneja Satèl·lit",                        //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Enable Limits",                             //English
    "Limits ein",                                //German
    "",                                          //Slovenian
    "Attiva Limiti",                             //Italian
    "Activeer Limiet",                           //Dutch
    "",                                          //Portugues
    "Activer Limites",                           //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Activa Limites",                            //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Activa Límits",                             //Catalan
    "¦çøÍÃüÁË úÞóòóøÊ",                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Disable Limits",                            //English
    "Limits aus",                                //German
    "",                                          //Slovenian
    "Disattiva Limiti",                          //Italian
    "De-activeer Limiet",                        //Dutch
    "",                                          //Portugues
    "Désactiver Limites",                        //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Desactiva Limites",                         //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Desactiva Límits",                          //Catalan
    "+ÁçøÍÃüÁË úÞóòóøÊ",                         //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Dish target: %d, position: %d",             //English
    "",                                          //German
    "",                                          //Slovenian
    "Obbiettivo: %d, posizione: %d",             //Italian
    "Schotel doel: %d, positie: %d",             //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Destino disco: %d, posición: %d",           //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Destinació disc: %d, posició: %d",          //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Channels found: %d, new: %d",               //English
    "",                                          //German
    "",                                          //Slovenian
    "Canali trovati: %d, nuovi: %d",             //Italian
    "Kanalen gevonden: %d, nieuw: %d",           //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Canales encontrados: %d, nuevos: %d",       //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Canals trobats: %d, nous: %d",              //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Position reached",                          //English
    "",                                          //German
    "",                                          //Slovenian
    "Posizione raggiunta",                       //Italian
    "Positie gehaald",                           //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Posición alcanzada",                        //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Arribat en posició",                        //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Motor wait",                                //English
    "",                                          //German
    "",                                          //Slovenian
    "Attesa motore",                             //Italian
    "Motor wacht",                               //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Espera motor",                              //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Espera motor",                              //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Motor error",                               //English
    "",                                          //German
    "",                                          //Slovenian
    "Errore motore",                             //Italian
    "Motorstoring",                              //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Error motor",                               //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Error motor",                               //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Position not set",                          //English
    "",                                          //German
    "",                                          //Slovenian
    "Posizione non impostata",                   //Italian
    "Positie niet gezet",                        //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Posición no ajustada",                      //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Posició no ajustada",                       //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Not positioned",                            //English
    "",                                          //German
    "",                                          //Slovenian
    "Non posizionato",                           //Italian
    "Niet gepositioneerd",                       //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "No posicionado",                            //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "No posicionat",                             //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Dish at limits",                            //English
    "",                                          //German
    "",                                          //Slovenian
    "Raggiunto limite",                          //Italian
    "Schotel op zijn limiet",                    //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Limite alcanzado",                          //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Arribat a límit",                           //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk
  },
  { "Position outside limits",                   //English
    "",                                          //German
    "",                                          //Slovenian
    "Posizione fuori dai limiti",                //Italian
    "Positie buiten de limieten",                //Dutch
    "",                                          //Portugues
    "",                                          //French
    "",                                          //Norwegian
    "",                                          //Finnish
    "",                                          //Polish
    "Fuera de los limites",                      //Spanish
    "",                                          //Greek
    "",                                          //Swedish
    "",                                          //Romanian
    "",                                          //Hugarian
    "Posició fora dels límits",                  //Catalan
    "",                                          //Russian
    "",                                          //Croatian
    "",                                          //Eesti
    "",                                          //Dansk  
  },
  { "Scanning, press any key to stop",                //English
    "",                                               //German
    "",                                               //Slovenian
    "Scansionando, premi un tasto per interrompere",  //Italian
    "Scannen, druk op een knop om te stoppen",        //Dutch
    "",                                               //Portugues
    "",                                               //French
    "",                                               //Norwegian
    "",                                               //Finnish
    "",                                               //Polish
    "Escaneando, pulse una tecla para interrumpir",   //Spanish
    "",                                               //Greek
    "",                                               //Swedish
    "",                                               //Romanian
    "",                                               //Hugarian
    "Escanejant, premi una tecla per a interrompre",  //Catalan
    "",                                               //Russian
    "",                                               //Croatian
    "",                                               //Eesti
    "",                                               //Dansk
  },
//Setup
  { "Card connected with motor",                //English
    "Karte, die mit Motor verbunden ist ",      //German
    "",                                         //Slovenian
    "Scheda collegata al motore",               //Italian
    "Kaart, verbonden met motor",               //Dutch
    "",                                         //Portugues
    "Carte, connectée au moteur",               //French
    "",                                         //Norwegian
    "",                                         //Finnish
    "",                                         //Polish
    "Tarjeta conectada al motor",               //Spanish
    "",                                         //Greek
    "",                                         //Swedish
    "",                                         //Romanian
    "",                                         //Hugarian
    "Tarja connectada al motor",                //Catalan
    "¦ïÞÁï À÷óòüùßùùïÎ À ú÷òðóÀ÷ý",             //Russian
    "",                                         //Croatian
    "",                                         //Eesti
    "",                                         //Dansk
  },
  { "Min. screen refresh time (ms)",            //English
    "",                                         //German
    "",                                         //Slovenian
    "Tempo min.aggiornam.schermo (ms)",         //Italian
    "Min. scherm ververstijd (ms)",             //Dutch
    "",                                         //Portugues
    "",                                         //French
    "",                                         //Norwegian
    "",                                         //Finnish
    "",                                         //Polish
    "Tiempo mín.refresco pantalla (ms)",        //Spanish
    "",                                         //Greek
    "",                                         //Swedish
    "",                                         //Romanian
    "",                                         //Hugarian
    "Temps mín.refresc pantalla (ms)",          //Catalan
    "",                                         //Russian
    "",                                         //Croatian
    "",                                         //Eesti
    "",                                         //Dansk
  },
  { "Are you sure?",                            //English
    "Sind sie sicher?",                         //German
    "",                                         //Slovenian
    "Sei sicuro?",                              //Italian
    "Weet u het zeker",                         //Dutch
    "",                                         //Portugues
    "Etes vous sur?",                           //French
    "",                                         //Norwegian
    "",                                         //Finnish
    "",                                         //Polish
    "¿Está seguro?",                            //Spanish
    "",                                         //Greek
    "",                                         //Swedish
    "",                                         //Romanian
    "",                                         //Hugarian
    "Està segur?",                              //Catalan
    "¦Ê ÖðóÞóùÊ?",                              //Russian
    "",                                         //Croatian
    "",                                         //Eesti
    "",                                         //Dansk
  },
  { "Actuator",                                 //English
    "",                                         //German
    "",                                         //Slovenian
    "Attuatore",                                //Italian
    "Actuator",                                 //Dutch
    "",                                         //Portugues
    "",                                         //French
    "",                                         //Norwegian
    "",                                         //Finnish
    "",                                         //Polish
    "Actuador",                                 //Spanish
    "",                                         //Greek
    "",                                         //Swedish
    "",                                         //Romanian
    "",                                         //Hugarian
    "Actuador",                                 //Catalan
    "",                                         //Russian
    "",                                         //Croatian
    "",                                         //Eesti
    "",                                         //Dansk
  },
  { "Linear or h-h actuator control",           //English
    "",                                         //German
    "",                                         //Slovenian
    "Controlla un attuatore lineare o h-h",     //Italian
    "Lineair of h-h actuator controle",         //Dutch
    "",                                         //Portugues
    "",                                         //French
    "",                                         //Norwegian
    "",                                         //Finnish
    "",                                         //Polish
    "Controla un actuador lineal o h-h",        //Spanish
    "",                                         //Greek
    "",                                         //Swedish
    "",                                         //Romanian
    "",                                         //Hugarian
    "Controla un actuador lineal o h-h",        //Catalan
    "",                                         //Russian
    "",                                         //Croatian
    "",                                         //Eesti
    "",                                         //Dansk
  },
  { NULL }
  };
