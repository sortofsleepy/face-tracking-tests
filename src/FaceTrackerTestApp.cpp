#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Surface.h"
#include "cinder/gl/Texture.h"
#include "Video.h"
#include "cinder/Log.h"
#include "cinder/Json.h"
#include "CinderOpenCv.h"


using namespace ci;
using namespace ci::app;
using namespace std;

class FaceTrackTestApp : public App {
  public:
    static void prepareSettings( Settings *settings ) {
        settings->setMultiTouchEnabled( false );
        
        settings->setWindowSize(1024, 768);
    }

    void setup() override;
    void updateFaces( const SurfaceRef &cameraImage );
    
    void fileDrop( FileDropEvent event ) override;

    void update() override;
    void draw() override;

    cv::CascadeClassifier    mFaceCascade, mEyeCascade;
    vector<Rectf>            mEyes;
    Video vid;
    gl::TextureRef testTexture;
};

void FaceTrackTestApp::setup()
{

   mFaceCascade.load( getAssetPath( "haarcascade_frontalface_alt.xml" ).string() );
   mEyeCascade.load( getAssetPath( "haarcascade_eye.xml" ).string() );    
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
    
    testTexture = gl::Texture::create(vid.getSurface());
    
    //gl::draw(vid.getTexture(),app::getWindowBounds());
    gl::draw(testTexture,vid.getCenteredRect());
    
}

void FaceTrackTestApp::updateFaces(const SurfaceRef &cameraImage){
    const int calcScale = 2; // calculate the image at half scale

    // create a grayscale copy of the input image
    cv::Mat grayCameraImage( toOcv( *cameraImage, CV_8UC1 ) );

    // scale it to half size, as dictated by the calcScale constant
    int scaledWidth = cameraImage->getWidth() / calcScale;
    int scaledHeight = cameraImage->getHeight() / calcScale;
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
            mEyes.push_back( eyeRect );
        }
    }
}

CINDER_APP( FaceTrackTestApp, RendererGl, FaceTrackTestApp::prepareSettings );
