function readArduinoData(src, ~)
    clc;
    % Read the ASCII data from the serialport object.
    data = readline(src);

    % Convert the string data to numeric type and save it in the UserData
    % property of the serialport object.
    data = split(data);
    src.UserData.Data(src.UserData.Count, 1) = str2double(data(1, :));
    src.UserData.Data(src.UserData.Count, 2) = str2double(data(2, :));

    % Update the Count value of the serialport object.
    src.UserData.Count = src.UserData.Count + 1;

    % If 1001 data points have been collected from the Arduino, switch off the
    % callbacks and plot the data.
    if src.UserData.Count > 64
        %configureCallback(src, "off");
        figure(105);
        plot(src.UserData.Data(1:32, 1), (1/128)*src.UserData.Data(1:32, 2).^2,'*-');
        title({'FFT Tacogramma' ['Tempo di acquisizione: ' num2str(src.UserData.Index*10) ' sec']});
        xlabel('freq (Hz)');
        ylabel('amplitude');
        src.UserData.Count = 1;
        src.UserData.Index = src.UserData.Index + 1;
    end
end
