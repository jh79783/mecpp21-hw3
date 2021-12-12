#include "pcpmain.h"

#include <random>

PCPMain::PCPMain(const std::string &color_name, const std::string &depth_name)
    : color_name(color_name), depth_name(depth_name)
{
    // TODO : add filter objects
    filters.push_back(std::make_unique<FirstFilter>());
}

void PCPMain::main()
{
    cv::Mat color = load_image(color_name, cv::IMREAD_COLOR);
    cv::Mat depth = load_image(depth_name, cv::IMREAD_ANYDEPTH);
    cv::Mat cloud = convert_to_point_cloud(depth);
    show_point_cloud(cloud, color);

    cv::Mat noisy_cloud = add_noise_cloud(cloud);
    show_point_cloud(noisy_cloud, color);
    for (auto &filter : filters)
    {
        cv::Mat smooth_cloud = smooth_filter(filter, noisy_cloud);
        evaluate_filter(cloud, smooth_cloud);
    }
}

cv::Mat PCPMain::load_image(const std::string &name, int imtype)
{
    std::string path = src_path + "/" + name;
    cv::Mat image = cv::imread(path, imtype);
    cout << "[load_image] shape=" << image.rows << " x " << image.cols
         << " x " << image.channels() << ", depth=" << image.depth() << "\n";
    return image;
}

cv::Mat PCPMain::convert_to_point_cloud(cv::Mat depth)
{
    cv::Mat cloud(depth.rows, depth.cols, CV_32FC3);
    for (int y = 0; y < depth.rows; ++y)
    {
        for (int x = 0; x < depth.cols; ++x)
        {
            float d = static_cast<float>(depth.at<uint16_t>(y, x)) / 1000.f;
            cv::Vec3f pt((x - camera_param.cx) / camera_param.fx * d,
                         (y - camera_param.cy) / camera_param.fy * d,
                         d);
            cloud.at<cv::Vec3f>(y, x) = pt;
            // if ((y % 100 == 50) && (x % 100 == 50))
            //     cout << "cloud (" << y << ", " << x << ") : [" << pt(0) << ", " << pt(1) << ", " << pt(2) << "]\n";
        }
    }
    return cloud;
}

void PCPMain::show_point_cloud(cv::Mat cloud, cv::Mat color)
{
    cv::viz::WCloud *wcloud;
    // create point cloud widget
    if (color.empty())
        wcloud = new cv::viz::WCloud(cloud, cv::viz::Color::white());
    else
        wcloud = new cv::viz::WCloud(cloud, color);

    cv::viz::Viz3d viewer = cv::viz::Viz3d("Point Cloud");
    viewer.showWidget("cloud", *wcloud);
    viewer.spin();
    delete wcloud;
}

cv::Mat PCPMain::smooth_filter(const std::unique_ptr<FilterBase> &filter, cv::Mat cloud)
{
    cv::Mat smooth_cloud = filter->apply(cloud);
    return smooth_cloud;
}

cv::Mat PCPMain::add_noise_cloud(cv::Mat cloud)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-0.02, 0.02);
    cv::Mat noisy_cloud = cloud.clone();
    for (int y = 0; y < noisy_cloud.rows; ++y)
    {
        for (int x = 0; x < noisy_cloud.cols; ++x)
        {
            cv::Vec3f noise(dis(gen), dis(gen), dis(gen));
            noisy_cloud.at<cv::Vec3f>(y, x) += noise;
        }
    }
    return noisy_cloud;
}

void PCPMain::evaluate_filter(cv::Mat cloud, cv::Mat smooth_cloud)
{
    cv::Vec3f diff;
    float max_diff = 0;
    float sum_diff = 0;
    for (int y = 0; y < cloud.rows; ++y)
    {
        for (int x = 0; x < cloud.cols; ++x)
        {
            diff = smooth_cloud.at<cv::Vec3f>(y, x) - cloud.at<cv::Vec3f>(y, x);
            for (int i = 0; i < 3; ++i)
            {
                float adi = fabs(diff(i));
                if (max_diff < adi)
                    max_diff = adi;
                sum_diff += adi;
            }
        }
    }
    float mean_diff = sum_diff / float(cloud.rows * cloud.cols);
    cout << "[evaluate] mean diff=" << mean_diff << ", max diff=" << max_diff << endl;
}
