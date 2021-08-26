#include <numeric>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "ParticleFilter.hpp"

ParticleFilter::ParticleFilter(const cv::Mat& occ, double _angle_incre, int pnum): 
    occupancy(occ), point_num(pnum), angle_incre(_angle_incre), rng(0)
{
    ray_num = std::round(2 * M_PI / angle_incre);
}

void ParticleFilter::particleInitialize(const cv::Mat& src) {
    int pt_num = 0;
    particles.clear();
    while (pt_num < point_num) {
        int x = rng.uniform(38, 1167);
        int y = rng.uniform(38, 867);
        if (src.at<uchar>(y, x) > 0x00) {
            particles.emplace_back(x, y);
            pt_num++;
        }
    }
}

void ParticleFilter::particleUpdate(double mx, double my) {
    for (Eigen::Vector2d& pt: particles) {
        double _mx = mx, _my = my;
        noisedMotion(_mx, _my);
        pt(0) += _mx;
        pt(1) += _my;
    }
}

void ParticleFilter::filtering(const std::vector<std::vector<cv::Point>>& obstacles, Eigen::Vector2d act_obs, cv::Mat& src) {
    cv::rectangle(src, walls, cv::Scalar(10, 10, 10), -1);
    cv::rectangle(src, floors, cv::Scalar(40, 40, 40), -1);
    cv::drawContours(src, obstacles, -1, cv::Scalar(10, 10, 10), -1);

    std::vector<double> weights, act_range;
    weights.resize(point_num, 0.0);
    act_range.resize(ray_num, -1.0);
    Volume act_vol;
    std::vector<Edge> act_egs;
    act_vol.calculateVisualSpace(obstacles, act_obs, src);
    act_vol.visualizeVisualSpace(obstacles, act_obs, src);
    act_vol.getValidEdges(act_egs);
    for (const Edge& eg: act_egs)
        edgeIntersect(eg, act_obs, act_range);
    scanPerturb(act_range);
    // visualizeRay(act_range, act_obs, src);
    #pragma omp parallel for num_threads(8)
    for (size_t i = 0; i < particles.size(); i++) {            // 计算weight
        const Eigen::Vector2d& pt = particles[i];
        int col = static_cast<int>(pt.x()), row = static_cast<int>(pt.y());
        if (occupancy.at<uchar>(row, col) == 0x00) {
            weights[i] = 1e-4;
            continue;
        }
        Volume vol;
        std::vector<Edge> edegs;
        std::vector<double> range;
        range.resize(ray_num, -1.0);
        vol.calculateVisualSpace(obstacles, pt, src);
        vol.getValidEdges(edegs);
        for (const Edge& eg: edegs)
            edgeIntersect(eg, pt, range);
        scanPerturb(range);
        double weight = probaComputation(act_range, range);
        weights[i] = weight;
    }
    double weight_sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    for (double& val: weights)                                  // 归一化形成概率
        val /= weight_sum;
    importanceResampler(weights);                               // 重采样
    visualizeParticles(weights, src);
}

void ParticleFilter::edgeIntersect(const Edge& eg, const Eigen::Vector2d& obs, std::vector<double>& range) {
    double angle_start = eg.front().z(), angle_end = eg.back().z();
    int id_start = static_cast<int>(ceil((angle_start + M_PI) / angle_incre)), 
        id_end = static_cast<int>(floor((angle_end + M_PI) / angle_incre));
    if (id_start == id_end + 1) return;
    if (id_start > id_end) {            // 奇异角度
        for (int i = id_start; i < ray_num; i++) {
            double angle = angle_incre * static_cast<double>(i) - M_PI;
            Eigen::Vector3d vec(cos(angle), sin(angle), angle);
            Eigen::Vector2d intersect = eg.getRayIntersect(vec, obs);
            range[i] = intersect.norm();
        }
        for (int i = 0; i <= id_end; i++) {
            double angle = angle_incre * static_cast<double>(i) - M_PI;
            Eigen::Vector3d vec(cos(angle), sin(angle), angle);
            Eigen::Vector2d intersect = eg.getRayIntersect(vec, obs);
            range[i] = intersect.norm();
        }
    } else {
        for (int i = id_start; i <= id_end; i++) {
            double angle = angle_incre * static_cast<double>(i) - M_PI;
            Eigen::Vector3d vec(cos(angle), sin(angle), angle);
            Eigen::Vector2d intersect = eg.getRayIntersect(vec, obs);
            range[i] = intersect.norm();
        }
    }
}

/// @ref implementation from Thrun: Probabilistic Robotics
void ParticleFilter::importanceResampler(const std::vector<double>& weights) {
    std::vector<Eigen::Vector2d> tmp;
    std::vector<int> tmp_ids;
    double dpoint_num = static_cast<double>(point_num);
    double r = rng.uniform(0.0, 1.0 / dpoint_num);
    double c = weights.front();
    int i = 0;
    for (int m = 1; m <= point_num; m++) {
        double u = r + static_cast<double>(m - 1) / dpoint_num;
        while (u > c) {
            i++;
            c += weights[i];
        }
        tmp.push_back(particles[i]);
    }
    particles.assign(tmp.begin(), tmp.end());
}

double ParticleFilter::probaComputation(const std::vector<double>& z, const std::vector<double>& exp_obs) {
    double s = 0.0;
    for (size_t i = 0; i < z.size(); i++) {
        s += std::abs(z[i] - exp_obs[i]);
    }
    s /= static_cast<double>(z.size());
    return 1.0 / (s + 1.0);
}

void ParticleFilter::scanPerturb(std::vector<double>& range) {
    for (double& val: range)
        val += rng.gaussian(7);
}

void ParticleFilter::visualizeRay(const std::vector<double>& range, const Eigen::Vector2d& obs, cv::Mat& dst) const {
    const cv::Point cv_obs(obs.x(), obs.y());
    for (int i = 0; i < ray_num; i++) {
        double angle = -M_PI + static_cast<double>(i) * angle_incre;
        Eigen::Vector2d ray = range[i] * Eigen::Vector2d(cos(angle), sin(angle)) + obs;
        cv::Point ray_end(ray.x(), ray.y());
        cv::line(dst, cv_obs, ray_end, cv::Scalar(0, 0, 255), 2);
    }
    cv::imwrite("../asset/ray.png", dst);
}

void ParticleFilter::visualizeParticles(const std::vector<double>& weights, cv::Mat& dst) const {
    Eigen::Vector2d center;
    double weight_sum = 0.0;
    center.setZero();
    for (size_t i = 0; i < particles.size(); i++) {
        const Eigen::Vector2d& pt = particles[i];
        center += weights[i] * pt;
        weight_sum += weights[i];
        double val = weights[i];
        uchar color_val = 254.0 * val, inv_color_val = 255.0 - color_val;
        cv::Scalar color(color_val, 0, inv_color_val);
        cv::circle(dst, cv::Point(pt.x(), pt.y()), 3, color, -1);
    }
    cv::circle(dst, cv::Point(center.x(), center.y()), 4, cv::Scalar(255, 0, 0), -1);
}
