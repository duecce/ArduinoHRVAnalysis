clear arduinoObj;
clc; 

% cambiare col corretto valore della porta COM e della velocità della seriale
arduinoObj = serialport("COM10", 115200); 

configureTerminator(arduinoObj, "CR/LF");
flush(arduinoObj); % pulisce i dati vecchi sulla seriale
arduinoObj.UserData = struct("Data", [], "Count", 1, "Index", 1); % 
configureCallback(arduinoObj, "terminator", @readArduinoData);
