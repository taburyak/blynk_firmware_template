# blynk_firmware_template
шаблон-конструктор для створення прошивок з WiFi Manager, WebOTA та автоматичним перепідключенням до вашого WiFi і серверу Blynk
-----------------------------------------------------------------------------------------------------------------------------------
Для зручності створив початковий шаблон-конструктор прошивки для системи Blynk з початковими налаштуваннями по WiFi зі смартфону (SSID, PASS, TOKEN, Name Device). З можливістю оновлювати прошивку по повітрю WebOTA, та перепідключенням до мережі WiFi і серверу Blynk у разі пропадання зв'язку. Плюс, якщо зв'язок буде втрачено, функціонал самого пристрою не буде "гальмувати", а буде працювати в режимі OFFLINE як слід, до наступного підключення до серверу. Є обробка натискання системної кнопки "FLASH" яка під'єднана до GPIO_0 і зазвичай присутня на всіх платах на базі "ESP8266" і пристроїв "IoT", наприклад, дуже популярних "Sonoff". Кнопка розпізнає довжину натискання. Коротке натискання - для свого коду, який буде щось вмикати/вимикати. Довге натискання від 5 до 10 секунд - перезавантаження пристрою. Довге натискання понад 10 секунд - скидання всіх налаштувань.
-----------------------------------------------------------------------------------------------------------------------------------
https://electronic-crafts.blogspot.com/2019/09/blynk-wifi-manager-webota-reconnect-template.html
