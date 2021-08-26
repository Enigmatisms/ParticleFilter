#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>
#include "lidarSim.hpp"
#include "mapEdit.h"

cv::Mat src;
Eigen::Vector2d obs;
Eigen::Vector2d orient;
double angle = 0.0;
const double K_P = 0.2;
const double K_I = 0.0001;
const double K_D = 0.001;
bool obs_set = false;
bool mouse_ctrl = false;
bool record_bag = false;

void on_mouse(int event, int x, int y, int flags, void *ustc) {
    if (event == cv::EVENT_LBUTTONDOWN && obs_set == false) {
        printf("cv::Point(%d, %d),\n", x, y);
        obs(0) = double(x);
        obs(1) = double(y);
        cv::circle(src, cv::Point(x, y), 3, cv::Scalar(0, 255, 0), -1);
        obs_set = true;
    } else if (obs_set == true) {
        orient(0) = double(x);
        orient(1) = double(y);
        if (event == cv::EVENT_LBUTTONDOWN) {
            printf("Now angle: %.4lf\n", angle * 180.0 / M_PI);
        }
    } 
}

double pidAngle(double now) {
    Eigen::Vector2d vec = orient - obs;
    double target = atan2(vec.x(), -vec.y());
    static double old_diff = 0.0, accum = 0.0;
    double diff = target - now;
    if (now > 2.5 && target < -2.5) {
        diff += 2 * M_PI;
    } else if (now < -2.5 && target > 2.5) {
        diff -= 2 * M_PI;
    }
    double result = K_P * diff + K_I * accum + K_D * (diff - old_diff);
    accum += diff;
    old_diff = diff;
    return result;
}

int main(int argc, char** argv) {
    cv::setNumThreads(4);
    std::vector<std::vector<cv::Point>> obstacles;
    std::string name = "test";
    if (argc < 2) {
        std::cerr << "Usage: ./main <Map name> <translation speed /pixel per move> <rotation speed / degree per move>\n";
        return -1;
    }
    name = std::string(argv[1]);
    mapLoad("../maps/" + name + ".txt", obstacles);
    src.create(cv::Size(1200, 900), CV_8UC3);
    cv::rectangle(src, walls, cv::Scalar(10, 10, 10), -1);
    cv::rectangle(src, floors, cv::Scalar(40, 40, 40), -1);
    cv::drawContours(src, obstacles, -1, cv::Scalar(10, 10, 10), -1);
    
    for (const Obstacle& egs: obstacles) {
        cv::circle(src, egs.front(), 3, cv::Scalar(0, 0, 255), -1);
        cv::circle(src, egs.back(), 3, cv::Scalar(255, 0, 0), -1);
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  
    cv::namedWindow("disp", cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback("disp", on_mouse, NULL);
    obs = Eigen::Vector2d(367, 769);
    int speed = 3;
    double rot_vel = 1.5;
    if (argc > 2) 
        speed = atoi(argv[2]);
    if (argc > 3)
        rot_vel = atof(argv[3]);
    rot_vel = rot_vel * M_PI / 180.0;
    while (obs_set == false) {
        cv::imshow("disp", src);
        char key = cv::waitKey(10);
        if (key == 27)
            return 0;
    }
    bool render_flag = true;
    double time_cnt = 1.0, time_sum = 0.0;
    double start_t = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
    double end_t = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
    time_sum += end_t - start_t;
    // cv::imwrite("../asset/thumbnail2.png", src);
    // std::string outPath = "/home/sentinel/cv_output.avi";
    // cv::Size sWH = cv::Size(1200, 900);
	// cv::VideoWriter outputVideo;
	// outputVideo.open(outPath, 1482049860, 1.5, sWH);	    // DIVX
    Eigen::Vector3d angles;
    angles << -M_PI / 2.0, M_PI / 2.0, M_PI / 1800.0;
    LidarSim ls(angles);
    printf("Main started.\n");
    int img_cnt = 0;
    std::vector<Eigen::Vector3d> gtt;       // ground truth tragectory
    while (true) {
        cv::imshow("disp", src);
        char key = cv::waitKey(1);
        bool break_flag = false;
        if (render_flag == true) {
            start_t = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
            ls.scan(obstacles, obs, src, angle);
            end_t = std::chrono::system_clock::now().time_since_epoch().count() / 1e9;
            // outputVideo.write(src);
            time_sum += end_t - start_t;
            time_cnt += 1.0;
            render_flag = false;
            // std::string name = "../asset/img" + std::to_string(img_cnt++) + ".png";
            // cv::imwrite(name.c_str(), src);
        }
        switch(key) {
            case 'w': {
                obs(0) += sin(angle) * speed;
                obs(1) -= cos(angle) * speed;
                render_flag = true;
                break;
            }
            case 'a': {
                obs(0) -= cos(angle) * speed;
                obs(1) -= sin(angle) * speed;
                render_flag = true;
                break;
            }
            case 's': {
                obs(0) -= sin(angle) * speed;
                obs(1) += cos(angle) * speed;
                render_flag = true;
                break;
            }
            case 'd': {
                obs(0) += cos(angle) * speed;
                obs(1) += sin(angle) * speed;
                render_flag = true;
                break;
            }
            case 'p': {
                if (mouse_ctrl == false)
                    angle += rot_vel;
                if (angle > M_PI)
                    angle -= 2 * M_PI;
                render_flag = true;
                break;
            }
            case 'o': {
                if (mouse_ctrl == false)
                    angle -= rot_vel;
                if (angle < -M_PI)
                    angle += 2 * M_PI;
                render_flag = true;
                break;
            }
            case 'm': {
                if (mouse_ctrl == false)
                    printf("Mouse angle control is on.\n");
                else 
                    printf("Mouse angle control is off.\n");
                mouse_ctrl = !mouse_ctrl;
                break;
            }
            case 27: break_flag = true;
        }
        if (mouse_ctrl == true) {
            angle += pidAngle(angle);
            if (angle > M_PI)
                angle -= 2 * M_PI;
            else if (angle < -M_PI)
                angle += 2 * M_PI;
            render_flag = true;
        }
        if (break_flag == true)
            break;
    }
    double mean_time = time_sum / time_cnt;
    printf("Average running time: %.6lf ms, fps: %.6lf hz\n", mean_time * 1e3, 1.0 / mean_time);
    cv::destroyAllWindows();
    // outputVideo.release();
    return 0;
}
