import { Component } from '@angular/core';
import { PrintDefinition } from '../model/datatypes';
import { PrintDefinitionService } from '../services/print-definition.service';

@Component({
  selector: 'app-print-definition-selector',
  template: `
    <ion-card *ngIf="collapsed" (click)="collapsed = false">
      <ion-card-header class="half-padding">
        Selected print output
      </ion-card-header>
      <ion-card-content>
        <ion-icon name="images" color="primary"></ion-icon>
        <ion-label class="ion-margin-start"> {{ getStringRepr(printDefinition) }} </ion-label>
      </ion-card-content>
    </ion-card>
    <ion-list *ngIf="!collapsed">
      <ion-list-header class="ion-no-padding" >
        <ion-searchbar
          placeholder="Search Print Definitions"
          (ionInput)="filterPrintDefinition($event)"
        ></ion-searchbar>
      </ion-list-header>
      <ion-item *ngFor="let pd of selectablePrintDefinitions" (click)="setSelected(pd)">
        <ion-label class="ion-margin-start"> {{ getStringRepr(pd) }} </ion-label>
        <ion-button color="success" icon-only><ion-icon name="create"></ion-icon></ion-button>
      </ion-item>
    </ion-list>
  `,
  styles: [`
    .half-padding {
      padding: 6px 16px !important;
    }
  `]
})
export class PrintDefinitionSelectorComponent {
  public printDefinition: PrintDefinition = new PrintDefinition('__unknown__', 'Loading ...');

  public _allPrintDefinitions: PrintDefinition[];

  public _selectablePrintDefinitions: PrintDefinition[];

  constructor(private pdService: PrintDefinitionService) {
    this._allPrintDefinitions = pdService.getAllPrintDefinitions();
    this.printDefinition = pdService.getSelectedPrintDefinition();

    pdService.printDefinitionSelected.subscribe(pd => {
      if (pd !== this.printDefinition) {
        this.printDefinition = pd;
      }
    });
  }

  private _collapsed = true;
  public get collapsed() {
    return this._collapsed;
  }
  public set collapsed(value: boolean) {
    if (value === this._collapsed) {
      return;
    }
    this._collapsed = value;
    this._selectablePrintDefinitions = this._allPrintDefinitions;
  }

  public get selectablePrintDefinitions() {
    return this._selectablePrintDefinitions != null
      ? this._selectablePrintDefinitions
      : this._allPrintDefinitions;
  }

  public setSelected(ps: PrintDefinition): void {
    this.pdService.setSelectedPrintDefinition(ps);
    this.collapsed = true;
  }

  filterPrintDefinition(evnt) {
    const text = evnt.target.value.toLowerCase();
    if (!text) {
      this._selectablePrintDefinitions = this._allPrintDefinitions;
      return;
    }
    this._selectablePrintDefinitions = this._allPrintDefinitions.filter(ps =>
      this.getStringRepr(ps)
        .toLowerCase()
        .includes(text)
    );
  }

  getStringRepr(pd: PrintDefinition): string {
    if (pd.text) {
      return pd.text;
    }
    const units = pd.units === 'inch' ? '???' : pd.units;
    return `${pd.height} x ${pd.width}${units} [${pd.resolution}dpi]`;
  }
}
