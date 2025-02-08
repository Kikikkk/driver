import time
import win32pipe
import win32file

pipe_name = r'\\.\pipe\emulated_com2'

def communicate_with_pipe():
    pipe = None  # Инициализация pipe
    try:
        # Попытка подключиться к pipe с повторными попытками
        for attempt in range(5):
            try:
                pipe = win32file.CreateFile(
                    pipe_name,
                    win32file.GENERIC_WRITE,
                    0,
                    None,
                    win32file.OPEN_EXISTING,
                    0,
                    None
                )
                print(f"Подключено к pipe: {pipe_name}")
                break
            except Exception as e:
                print(f"Ошибка открытия pipe (попытка {attempt + 1} из 5): {e}")
                time.sleep(1)
        else:
            print("Не удалось подключиться к pipe после 5 попыток.")
            return

        # Данные для отправки
        data = "SimulatedData"
        try:
            response, nbytes = win32file.WriteFile(pipe, data.encode('utf-8'))  # Кодируем данные в байты
            print(f"Отправлено: {data}")
            print(f"Возвращенный ответ: {response}\nЗаписано байтов: {nbytes}")
        except Exception as e:
            print(f"Ошибка записи в pipe: {e}")
    finally:
        # Закрываем дескриптор канала, если он был открыт
        if pipe:
            win32file.CloseHandle(pipe)
            print("Pipe закрыт.")

if __name__ == "__main__":
    communicate_with_pipe()
