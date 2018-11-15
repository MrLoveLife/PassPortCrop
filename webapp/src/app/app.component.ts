import { Component, ElementRef, OnInit } from '@angular/core';
import { Http, ResponseContentType } from '@angular/http';

import { CrownChinPointPair, TiledPhotoRequest, PassportStandard, UnitType, Canvas } from './model/datatypes';
import { BackEndService } from './services/back-end.service';
import { Plugins } from '@capacitor/core';


@Component({
    selector: 'app-root',
    templateUrl: './app.component.html',
    styles: [`
     .fit {
        max-width: 99%;
        max-height: 99%;
      }`]
})
export class AppComponent implements OnInit {
    echoString = 'Welcome to this app';

    imageKey: string;
    imageSrc: string | ArrayBuffer = '#';
    outImgSrc: any = '#';

    // Model data
    crownChinPointPair: CrownChinPointPair;
    passportStandard: PassportStandard = new PassportStandard(
        35, 45, 34, UnitType.mm
    );
    canvas: Canvas = {
        height: 4,
        width: 6,
        resolution: 300,
        units: UnitType.inch
    };

    constructor(
        public el: ElementRef,
        private beService: BackEndService,
        private http: Http) {
    }

    ngOnInit(): void {

        const { PppPlugin } = Plugins;
        PppPlugin.echo({ value: 'aaa' }).then(v => {
            this.echoString = v.value;
        });

        this.http.get('assets/config.json').subscribe(r => {
            let objcfg = r.json();

            this.http.get('assets/sp_model.dat', {
                responseType: ResponseContentType.Blob
            }).subscribe(rr => {
                const reader = new FileReader();
                reader.readAsDataURL(rr.blob());
                reader.onloadend = () => {
                    //let content64 = reader.result as string;
                   // content64 = content64.substring(content64.indexOf(',') + 1);
                    //objcfg.shapePredictor.data = content64;
                    let config = JSON.stringify(objcfg);

                    objcfg = null; // memory cleanup
                    this.echoString = 'dddddddd!';
                    PppPlugin.configure({ cfg: config }).then(() => {
                        config = null; // memory cleanup
                        this.echoString = 'Configured!';
                    });
                };
            });
        });
    }

    loadImage(event) {
        const fileList: FileList = event.target.files;
        if (fileList && fileList[0]) {
            const file = fileList[0];
            this.crownChinPointPair = null;
            // Upload the file to the server to detect landmarks
            this.beService.uploadImageToServer(file).then(imgKey => {
                this.imageKey = imgKey;
                this.retrieveLandmarks();
            });
            // Read the image and display it
            const reader = new FileReader();
            reader.onload = () => {
                const imgdata = reader.result;
                this.imageSrc = imgdata;
            };
            reader.readAsDataURL(file);
        }
    }

    retrieveLandmarks() {
        this.beService.retrieveLandmarks(this.imageKey).then((landmarks) => {
            if (landmarks.errorMsg) {
                console.log(landmarks.errorMsg);
            } else {
                if (landmarks.crownPoint && landmarks.chinPoint) {
                    console.log('Landmarks calculated.');
                    this.crownChinPointPair = landmarks;
                }
            }
        });
    }

    onLandmarksEdited() {
    }

    createPrint() {
        console.log('Creating print output');
        const req = new TiledPhotoRequest(this.imageKey, this.passportStandard,
            this.canvas, this.crownChinPointPair);
        this.beService.getTiledPrint(req).then(outputDataUrl => {
            this.outImgSrc = outputDataUrl;
        });
    }
}
