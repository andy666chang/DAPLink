
python -m venv .venv

call .venv\Scripts\activate.bat

pip install west

west init -l tools
west update

west zephyr-export

pip install -r zephyr-rtos\scripts\requirements.txt
