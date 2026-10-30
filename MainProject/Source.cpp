#include <iostream>
#include <opencv2/opencv.hpp>
#include <windows.h>
#include <vector>
#include <string>
#include <cmath>

int main() {
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	// создаем класс видео для встроенной камеры
	// проверяем на открытие видеокамеры
	cv::VideoCapture video(0);
	if (video.isOpened() == false) {
		std::cerr << "Video isn't opened" << std::endl;
		return -1;
	}
	// создаём окна
	// создаем матрицу для фрейма, ядра 
	// и два скаляра для оранжевого цвета
	cv::namedWindow("Frame");
	cv::namedWindow("Controls");
	cv::Mat frame;
	int hLow = 5, sLow = 100, vLow = 100, hHigh = 15, sHigh = 255, vHigh = 255; // обнаружение оранжевого цвета
	// изменение цвета с помощью трекбара
	cv::createTrackbar("H low", "Controls", &hLow, 179, 0);
	cv::createTrackbar("S low", "Controls", &sLow, 225, 0);
	cv::createTrackbar("V low", "Controls", &vLow, 225, 0);
	cv::createTrackbar("H high", "Controls", &hHigh, 179, 0);
	cv::createTrackbar("S high", "Controls", &sHigh, 225, 0);
	cv::createTrackbar("V high", "Controls", &vHigh, 225, 0);
	int kernelSize = 5, safeSize;
	cv::createTrackbar("KernelSize", "Controls", &kernelSize, 31, 0);
	// пишем вектор из векторов из точек для обозначения контура
	std::vector<std::vector<cv::Point>> contours;
	float smoothX = 0, smoothY = 0, prevX = 0, prevY = 0;
	bool hasTarget = false;
	int framesLost = 0;
	int64 prevTick = 0;
	// основной цикл
	// сразу проверяем на читку кадров с вебки 
	// и записываем в буллевую переменную
	// если всё норм то идёт основной кодик
	while (true) {
		bool isSuccess = video.read(frame);
		if (isSuccess) {
			// объявляем переменные для нахождения наибольшей площади
			// создаем матрицы зеркалки, HSV, маски и маски сглаженной
			double bestArea = 0;
			int bestIndex = -1;
			cv::Mat flippedFrame;
			cv::Mat hsv;
			cv::Mat mask;
			cv::Mat cleanMask;
			cv::flip(frame, flippedFrame, 1); // зеркалим и записываем в флиппедфрейм
			cv::Scalar lower_orange(hLow, sLow, vLow);
			cv::Scalar upper_orange(hHigh, sHigh, vHigh);
			safeSize = kernelSize;
			if (safeSize % 2 == 0) {
				safeSize++;
			}
			cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(safeSize, safeSize));
			cv::cvtColor(flippedFrame, hsv, cv::COLOR_BGR2HSV); // меняем бгр на хсв
			cv::inRange(hsv, lower_orange, upper_orange, mask); // устанавливаем границы хсв
			cv::erode(mask, cleanMask, kernel); // чистим маску
			cv::dilate(cleanMask, cleanMask, kernel); // чистим маску х2
			cv::findContours(cleanMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE); // ищем контур белых объектов в очищенной маске и кидаем в список
			// проходимся по списку до конца и выделяем больший контур
			for (int i = 0; i < contours.size(); i++) {
				double area = cv::contourArea(contours[i]);
				if (area > bestArea) {
					bestArea = area;
					bestIndex = i;
				}
			}
			// после нахождения большего контура строим рамку вокруг
			if (bestIndex != -1) {
				framesLost = 0; // устанавливем фреймлост на нуль, ибо у нас объект видно на камере
				cv::Rect box = cv::boundingRect(contours[bestIndex]); // создаем рамку, вычисляем координаты минимального прямоугольника, в который помещается объект
				cv::rectangle(flippedFrame, box, cv::Scalar(0, 255, 0), 2); // строим рамку
				// вычисляем координаты центра
				int cx = box.x + box.width / 2; 
				int cy = box.y + box.height / 2;
				// устанавливаем координаты для сравнения с прошлыми
				if (!hasTarget) {
					smoothX = cx;
					smoothY = cy;
					hasTarget = true;
					prevTick = cv::getTickCount();
				}
				// в самом начале, когда объект только появился, устанавливаем его координаты начальные с последующими на один кадр
				else {
					prevX = smoothX;
					prevY = smoothY;
					smoothX = 0.15 * cx + (1 - 0.15) * smoothX;
					smoothY = 0.15 * cy + (1 - 0.15) * smoothY;
					int64 now = cv::getTickCount();
					// считываем скорость объекта
					double elapsed = (now - prevTick) / cv::getTickFrequency();
					prevTick = now;
					float distance = sqrt((int)(smoothX - prevX) * (int)(smoothX - prevX) + (int)(smoothY - prevY) * (int)(smoothY - prevY));
					float speed = distance / elapsed;
					cv::Point coordTextS(box.x, box.y - 40);
					cv::putText(flippedFrame, std::to_string(speed), coordTextS, cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);

					// вставляем кружок в центр объекта и координаты
					std::string coord = "(" + std::to_string(smoothX) + ", " + std::to_string(smoothY) + ")";
					cv::Point coordText(box.x, box.y - 10);
					cv::putText(flippedFrame, coord, coordText, cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
					cv::Point center((int)smoothX, (int)smoothY);
					cv::circle(flippedFrame, center, 5, cv::Scalar(0, 255, 0), -1);
				}
			}
			else {
				// объект не видно - суммируем фреймы его отсутствия
				// фреймов больше 30 - убираем координаты и рамку
				framesLost++;
				if (framesLost < 30) {
					std::string coord = "(" + std::to_string(smoothX) + ", " + std::to_string(smoothY) + ")";
					cv::Point coordText(smoothX, smoothY - 10);
					cv::putText(flippedFrame, coord, coordText, cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
					cv::Point center((int)smoothX, (int)smoothY);
					cv::circle(flippedFrame, center, 5, cv::Scalar(0, 255, 0), -1);
				}

			}
			// показываем все окошки
			cv::imshow("Frame", flippedFrame);
			//cv::imshow("HSV", hsv);
			//cv::imshow("Mask", mask);
			cv::imshow("CleanMask", cleanMask);
		}
		else {
			std::cerr << "Video camera is disconnected" << std::endl;
			break;
		}
		// всё вырубаем
		int key = cv::waitKey(20);
		if (key == 'q') {
			std::cout << "q is pressed by the user. Stopping the video" << std::endl;
			break;
		}
	}
	video.release();
	cv::destroyAllWindows();
}