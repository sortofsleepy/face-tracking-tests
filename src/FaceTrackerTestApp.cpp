#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Surface.h"
#include "cinder/gl/Texture.h"
#include "Video.h"
#include "cinder/Log.h"
#include "cinder/Json.h"
#include "CinderOpenCv.h"
#include "cinder/Thread.h"
#include "cinder/Json.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct EyeFrame {
  
    int frameNumber;
    ci::vec2 eye1,eye2;
    
    EyeFrame(int num, vec2 e1,vec2 e2){
        frameNumber = num;
        eye1 = e1;
        eye2 = e2;
    }
};

class FaceTrackTestApp : public App {
  public:
    ~FaceTrackTestApp();
    static void prepareSettings( Settings *settings ) {
        settings->setMultiTouchEnabled( false );
        
        settings->setWindowSize(1024, 768);
    }

    void setup() override;
    void updateFaces(const Surface cameraImage);
    void updateProfileFaces(const Surface cameraImage);
    void fileDrop( FileDropEvent event ) override;

    void update() override;
    void draw() override;
    
    std::thread mThread;
    bool threadStarted;
    
    // will write out eye positions each frame.
    JsonTree doc;
    
    //classifiers for faces and eyes
    cv::CascadeClassifier    mFaceCascade, mEyeCascade,mProfileCascade;
    
    //! Holds eye information.
    vector<Rectf> mEyes;
    //vector<Rectf> mFaces;
    
    vector<EyeFrame> frames;
    
    int frameCount = 0;
    
    bool jsonWritten;
    
    //! Manages vide playback.
    Video vid;
    
    //! texture to render output to for reference.
    gl::TextureRef renderTexture;
    
};

FaceTrackTestApp::~FaceTrackTestApp(){
    mThread.join();
}

void FaceTrackTestApp::setup()
{

    mFaceCascade.load( getAssetPath( "haarcascade_frontalface_alt.xml" ).string() );
    mEyeCascade.load( getAssetPath( "haarcascade_eye.xml" ).string() );
    mProfileCascade.load(getAssetPath("lbpcascade_profileface.xml").string());
    
    
    threadStarted = false;
    
    vid.setOnComplete([=]()->void{
        doc.clear();
        
        
        for(int i = 0; i < frames.size(); ++i){
            JsonTree entry;
            JsonTree frame = JsonTree("frame",frames[i].frameNumber);
            JsonTree eye1 = JsonTree("eye1", " " + std::to_string(frames[i].eye1.x) + "|" + std::to_string(frames[i].eye1.y));
            
            JsonTree eye2 = JsonTree("eye2", " " + std::to_string(frames[i].eye2.x) + "|" + std::to_string(frames[i].eye2.y));
            
            entry.pushBack(frame);
            entry.pushBack(eye1);
            entry.pushBack(eye2);
            
            doc.pushBack(entry);
            
        }
        
        // get output path, default to documents
        fs::path p("~/Documents/test.json");
        // write file.
        doc.write(p,JsonTree::WriteOptions());
        
        
    });
}


void FaceTrackTestApp::fileDrop( FileDropEvent event )
{
    vid.loadVideo(event.getFile(0));
}

void FaceTrackTestApp::update()
{
    
    vid.update();
   
    
}

void FaceTrackTestApp::draw()
{
    gl::clear( Color( 0, 0, 0 ) );
    gl::viewport(app::getWindowSize());
    gl::setMatricesWindow(app::getWindowSize());
    

    frameCount += 1;
    
    if(vid.isVideoLoaded() && vid.isPlaying()){
        renderTexture = gl::Texture::create(vid.getSurface());
        updateFaces(vid.getSurface());
        
        // start thread to calculate information about faces / eyes.
        if(!threadStarted){
            mThread = thread( bind( &FaceTrackTestApp::updateFaces, this, std::placeholders::_1 ),vid.getSurface() );
            
            // set flag so we don't repeatedly start a new thread.
            threadStarted = true;
        }
        
        gl::pushMatrices();
        gl::translate(vec2(vid.getCenteredRect().getX1(),0));
    
        gl::draw(renderTexture);
        for( vector<Rectf>::const_iterator eyeIter = mEyes.begin(); eyeIter != mEyes.end(); ++eyeIter ){
            vec2 pos = eyeIter->getCenter();
            //Rectf center = vid.getCenteredRect();
            
            // center the eyes
            //pos.x += center.getX1();
            
            
            gl::drawSolidCircle(pos, eyeIter->getWidth() / 2 );
        }
        gl::popMatrices();
        // store reference to where the eyes were for each frame so we can write to json after the video is done.
        if(mEyes.size() == 2){
            frames.push_back(EyeFrame(frameCount,mEyes[0].getCenter(),mEyes[1].getCenter()));
        }
    
    }

    
}

void FaceTrackTestApp::updateProfileFaces(const Surface cameraImage){
    const int calcScale = 2; // calculate the image at half scale
    
    
    if(cameraImage.getWidth() <= 0 || !vid.isPlaying()){
        return;
    }
    
    // create a grayscale copy of the input image
    cv::Mat grayCameraImage( toOcv( cameraImage, CV_8UC1 ) );
    int scaledWidth = cameraImage.getWidth() / calcScale;
    int scaledHeight = cameraImage.getHeight() / calcScale;
    
    cv::Mat smallImg( scaledHeight, scaledWidth, CV_8UC1 );
    cv::resize( grayCameraImage, smallImg, smallImg.size(), 0, 0, cv::INTER_LINEAR );
       
    // equalize the histogram
    cv::equalizeHist( smallImg, smallImg );

    // clear out the previously deteced faces & eyes
    //mEyes.clear();

    // detect the faces and iterate them, appending them to mFaces
    vector<cv::Rect> faces;
    mProfileCascade.detectMultiScale( smallImg, faces );
    for( vector<cv::Rect>::const_iterator faceIter = faces.begin(); faceIter != faces.end(); ++faceIter ) {
            Rectf faceRect( fromOcv( *faceIter ) );
            faceRect *= calcScale;
            //mFaces.push_back( faceRect );
           
            // detect eyes within this face and iterate them, appending them to mEyes
            vector<cv::Rect> eyes;
            mEyeCascade.detectMultiScale( smallImg( *faceIter ), eyes );
           
            for( vector<cv::Rect>::const_iterator eyeIter = eyes.begin(); eyeIter != eyes.end(); ++eyeIter ) {
                    Rectf eyeRect( fromOcv( *eyeIter ) );
                    eyeRect = eyeRect * calcScale + faceRect.getUpperLeft();
                    // limit things to when we find exactly 2 so we can try to filter out a bit of
                    // the noise.
                    if(eyes.size() == 2){
                        mEyes.push_back( eyeRect );
                    }
            }
    }
    
}
void FaceTrackTestApp::updateFaces(const Surface cameraImage){
    const int calcScale = 2; // calculate the image at half scale
    
    
    if(cameraImage.getWidth() <= 0 || !vid.isPlaying()){
        return;
    }
    
    // create a grayscale copy of the input image
    cv::Mat grayCameraImage( toOcv( cameraImage, CV_8UC1 ) );
    int scaledWidth = cameraImage.getWidth() / calcScale;
    int scaledHeight = cameraImage.getHeight() / calcScale;
    
    cv::Mat smallImg( scaledHeight, scaledWidth, CV_8UC1 );
    cv::resize( grayCameraImage, smallImg, smallImg.size(), 0, 0, cv::INTER_LINEAR );
       
    // equalize the histogram
    cv::equalizeHist( smallImg, smallImg );

    // clear out the previously deteced faces & eyes
    mEyes.clear();

    // detect the faces and iterate them, appending them to mFaces
    vector<cv::Rect> faces;
    mFaceCascade.detectMultiScale( smallImg, faces );
    for( vector<cv::Rect>::const_iterator faceIter = faces.begin(); faceIter != faces.end(); ++faceIter ) {
            Rectf faceRect( fromOcv( *faceIter ) );
            faceRect *= calcScale;
            //mFaces.push_back( faceRect );
           
            // detect eyes within this face and iterate them, appending them to mEyes
            vector<cv::Rect> eyes;
            mEyeCascade.detectMultiScale( smallImg( *faceIter ), eyes );
           
            for( vector<cv::Rect>::const_iterator eyeIter = eyes.begin(); eyeIter != eyes.end(); ++eyeIter ) {
                    Rectf eyeRect( fromOcv( *eyeIter ) );
                    eyeRect = eyeRect * calcScale + faceRect.getUpperLeft();
                    // limit things to when we find exactly 2 so we can try to filter out a bit of
                    // the noise.
                    if(eyes.size() == 2){
                        mEyes.push_back( eyeRect );
                    }
            }
    }
    
    //updateProfileFaces(cameraImage);
}

CINDER_APP( FaceTrackTestApp, RendererGl, FaceTrackTestApp::prepareSettings );
